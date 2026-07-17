/*
 * pybind11 bindings for libpst.
 *
 * A small, self-contained high-level API over the libpst C core: open a
 * .pst file, walk its folder tree, and read each message's subject, sender,
 * date, plain-text body and attachment file names. The libpst C sources are
 * compiled straight into this extension module (see CMakeLists.txt), so the
 * built wheel has no runtime dependency on a separate libpst shared library.
 *
 * This replaces the older Boost.Python binding (python/python-libpst.cpp).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <cstring>
#include <string>
#include <vector>

extern "C" {
#include "libpst.h"
#include "timeconv.h"
#include "libstrfunc.h"
#include "vbuf.h"
#include "lzfu.h"
}

namespace py = pybind11;

namespace {

// Guard against pathological / malformed folder trees making a single open()
// recurse without bound. Real folder hierarchies are shallow.
constexpr int kMaxFolderDepth = 128;

std::string str_or_empty(const pst_string &s) {
    return s.str ? std::string(s.str) : std::string();
}

// Convert a message field to UTF-8 (in place) and return it as a std::string.
std::string utf8_field(pst_item *item, pst_string *field) {
    if (!field || !field->str) return std::string();
    pst_convert_utf8_null(item, field);
    return field->str ? std::string(field->str) : std::string();
}

std::string filetime_iso(const FILETIME *ft) {
    if (!ft) return std::string();
    char buf[64];
    // pst_rfc2425_datetime_format writes an RFC2425 (ISO-8601-ish) timestamp.
    char *r = pst_rfc2425_datetime_format(ft, (int)sizeof(buf), buf);
    return r ? std::string(r) : std::string();
}

struct Attachment {
    std::string filename;
    std::string mimetype;
    std::size_t size = 0;
};

struct Message {
    std::string subject;
    std::string sender;
    std::string sender_name;
    std::string date;        // RFC2425 / ISO-8601, empty if unknown
    std::string message_id;
    std::string plain_text;  // PR_BODY
    std::vector<Attachment> attachments;
};

struct Folder {
    std::string name;
    std::vector<Folder> subfolders;
    std::vector<Message> messages;
};

Message extract_message(pst_file *pf, pst_item *item) {
    (void)pf;
    Message m;
    m.subject    = utf8_field(item, &item->subject);
    m.plain_text = utf8_field(item, &item->body);
    if (item->email) {
        m.sender      = utf8_field(item, &item->email->sender_address);
        m.sender_name = utf8_field(item, &item->email->outlook_sender_name);
        m.message_id  = utf8_field(item, &item->email->messageid);
        const FILETIME *when = item->email->arrival_date
                             ? item->email->arrival_date
                             : item->email->sent_date;
        m.date = filetime_iso(when);
    }
    for (pst_item_attach *a = item->attach; a; a = a->next) {
        Attachment at;
        // Prefer the long file name, fall back to the short one.
        std::string fn = utf8_field(item, &a->filename2);
        if (fn.empty()) fn = utf8_field(item, &a->filename1);
        at.filename = fn;
        at.mimetype = utf8_field(item, &a->mimetype);
        at.size     = a->data.size;
        m.attachments.push_back(std::move(at));
    }
    return m;
}

void collect(pst_file *pf, pst_desc_tree *d_ptr, Folder &parent, int depth) {
    if (depth > kMaxFolderDepth) return;
    for (; d_ptr; d_ptr = d_ptr->next) {
        if (!d_ptr->desc) continue;
        pst_item *item = pst_parse_item(pf, d_ptr, NULL);
        if (!item) continue;

        if (item->folder && item->file_as.str) {
            Folder f;
            f.name = utf8_field(item, &item->file_as);
            if (d_ptr->child) collect(pf, d_ptr->child, f, depth + 1);
            parent.subfolders.push_back(std::move(f));
        } else if (item->type == PST_TYPE_NOTE && item->email) {
            parent.messages.push_back(extract_message(pf, item));
        }
        pst_freeItem(item);
    }
}

class PstFile {
public:
    PstFile(const std::string &path, const std::string &charset) {
        const char *cs = charset.empty() ? "UTF-8" : charset.c_str();
        if (pst_open(&pf_, path.c_str(), cs) != 0) {
            throw std::runtime_error("failed to open pst file: " + path);
        }
        opened_ = true;
        if (pst_load_index(&pf_) != 0) {
            // A C++ constructor that throws does not run this object's own
            // destructor, so release the open pst_file here before throwing.
            close();
            throw std::runtime_error("failed to load index (not a valid pst?): " + path);
        }
        pst_load_extended_attributes(&pf_);

        pst_item *root_item = pf_.d_head ? pst_parse_item(&pf_, pf_.d_head, NULL) : NULL;
        if (root_item) {
            pst_desc_tree *top = pst_getTopOfFolders(&pf_, root_item);
            root_.name = "ROOT";
            if (top && top->child) collect(&pf_, top->child, root_, 0);
            pst_freeItem(root_item);
        }
    }

    ~PstFile() { close(); }

    void close() {
        if (opened_) {
            pst_close(&pf_);
            opened_ = false;
        }
    }

    const Folder &root() const { return root_; }

private:
    pst_file pf_{};
    bool opened_ = false;
    Folder root_;
};

}  // namespace

PYBIND11_MODULE(_core, m) {
    m.doc() = "pybind11 bindings for libpst: read Microsoft Outlook .pst files";
    m.attr("__version__") = "0.1.0";

    py::class_<Attachment>(m, "Attachment")
        .def_readonly("filename", &Attachment::filename)
        .def_readonly("mimetype", &Attachment::mimetype)
        .def_readonly("size", &Attachment::size)
        .def("__repr__", [](const Attachment &a) {
            return "<Attachment filename='" + a.filename + "' size=" + std::to_string(a.size) + ">";
        });

    py::class_<Message>(m, "Message")
        .def_readonly("subject", &Message::subject)
        .def_readonly("sender", &Message::sender)
        .def_readonly("sender_name", &Message::sender_name)
        .def_readonly("date", &Message::date)
        .def_readonly("message_id", &Message::message_id)
        .def_readonly("plain_text", &Message::plain_text)
        .def_readonly("attachments", &Message::attachments)
        .def_property_readonly("attachment_names", [](const Message &m) {
            std::vector<std::string> names;
            for (const auto &a : m.attachments) names.push_back(a.filename);
            return names;
        })
        .def("__repr__", [](const Message &m) {
            return "<Message subject='" + m.subject + "' sender='" + m.sender + "'>";
        });

    py::class_<Folder>(m, "Folder")
        .def_readonly("name", &Folder::name)
        .def_readonly("subfolders", &Folder::subfolders)
        .def_readonly("messages", &Folder::messages)
        .def("__repr__", [](const Folder &f) {
            return "<Folder name='" + f.name + "' subfolders=" +
                   std::to_string(f.subfolders.size()) + " messages=" +
                   std::to_string(f.messages.size()) + ">";
        });

    py::class_<PstFile>(m, "PstFile")
        .def(py::init<const std::string &, const std::string &>(),
             py::arg("path"), py::arg("charset") = "UTF-8")
        .def_property_readonly("root", &PstFile::root,
                               py::return_value_policy::reference_internal)
        .def("close", &PstFile::close)
        .def("__enter__", [](PstFile &self) -> PstFile & { return self; })
        .def("__exit__", [](PstFile &self, py::object, py::object, py::object) {
            self.close();
            return false;
        });

    m.def("open", [](const std::string &path, const std::string &charset) {
        return new PstFile(path, charset);
    }, py::arg("path"), py::arg("charset") = "UTF-8",
       "Open a .pst file and return a PstFile whose .root is the folder tree.");
}
