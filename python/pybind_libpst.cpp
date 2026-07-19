/*
 * pybind11 bindings for libpst.
 *
 * A self-contained high-level API over the libpst C core: open a .pst file,
 * walk its folder tree, and read messages, contacts, appointments and journal
 * entries, including attachment bodies. The libpst C sources are compiled
 * straight into this extension module (see CMakeLists.txt), so the built wheel
 * has no runtime dependency on a separate libpst shared library.
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

#include <cstdlib>
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

// Decompress an item's PR_RTF_COMPRESSED body to plain RTF text. Returns empty
// if there is no RTF body or decompression fails.
std::string rtf_text(const pst_binary &rtf) {
    if (!rtf.data || rtf.size == 0) return std::string();
    size_t out_size = 0;
    char *out = pst_lzfu_decompress(rtf.data, (uint32_t)rtf.size, &out_size);
    if (!out) return std::string();
    std::string s(out, out_size);
    free(out);
    return s;
}

struct Attachment {
    std::string filename;
    std::string mimetype;
    std::string content_id;
    std::size_t size = 0;
    int method = 0;
    int position = -1;
    std::string data;  // raw attachment bytes (may be empty)
};

struct Message {
    std::string subject;
    std::string sender;         // sender_address
    std::string sender_name;    // outlook_sender_name
    std::string to;             // sentto_address (PR_DISPLAY_TO)
    std::string cc;             // cc_address
    std::string bcc;            // bcc_address
    std::string reply_to;
    std::string in_reply_to;
    std::string message_id;
    std::string date;           // RFC2425 / ISO-8601, empty if unknown
    std::string sent_date;
    std::string header;         // full transport header block
    std::string plain_text;     // PR_BODY
    std::string html;           // PR_HTML (htmlbody)
    std::string rtf;            // decompressed PR_RTF_COMPRESSED
    std::string comment;
    int importance = -1;        // 0 low / 1 normal / 2 high
    int priority = 0;           // -1 nonurgent / 0 normal / 1 urgent
    int sensitivity = -1;       // 0 none / 1 personal / 2 private / 3 confidential
    std::vector<Attachment> attachments;
};

struct Contact {
    std::string display_name;   // file_as
    std::string fullname;
    std::string first_name;
    std::string surname;
    std::string nickname;
    std::string company_name;
    std::string job_title;
    std::string department;
    std::string email1;         // address1
    std::string email2;         // address2
    std::string email3;         // address3
    std::string business_phone;
    std::string home_phone;
    std::string mobile_phone;
    std::string business_address;
    std::string home_address;
};

struct Appointment {
    std::string subject;
    std::string location;
    std::string start;          // ISO-8601, empty if unknown
    std::string end;
    std::string timezone;
    std::string body;           // PR_BODY
    bool all_day = false;
    int busy_status = -1;       // showas: 0 free / 1 tentative / 2 busy / 3 oof
    bool is_recurring = false;
    int recurrence_type = 0;    // 0 none / 1 daily / 2 weekly / 3 monthly / 4 yearly
    std::string recurrence_description;
};

struct Journal {
    std::string subject;
    std::string type;
    std::string description;
    std::string start;
    std::string end;
};

struct Folder {
    std::string name;
    std::vector<Folder> subfolders;
    std::vector<Message> messages;
    std::vector<Contact> contacts;
    std::vector<Appointment> appointments;
    std::vector<Journal> journals;
};

Attachment extract_attachment(pst_file *pf, pst_item *item, pst_item_attach *a) {
    Attachment at;
    // Prefer the long file name, fall back to the short one.
    std::string fn = utf8_field(item, &a->filename2);
    if (fn.empty()) fn = utf8_field(item, &a->filename1);
    at.filename   = fn;
    at.mimetype   = utf8_field(item, &a->mimetype);
    at.content_id = utf8_field(item, &a->content_id);
    at.method     = a->method;
    at.position   = a->position;

    // pst_attach_to_mem transfers ownership of the (possibly reference-resolved)
    // attachment buffer to us: it hands back a malloc'd buffer and NULLs the
    // in-item copy so pst_freeItem won't double free. Call it exactly once,
    // copy into a Python-owned string, then free the C buffer.
    pst_binary bin = pst_attach_to_mem(pf, a);
    if (bin.data && bin.size) {
        at.data.assign(bin.data, bin.size);
        at.size = bin.size;
        free(bin.data);
    } else {
        at.size = a->data.size;
    }
    return at;
}

Message extract_message(pst_file *pf, pst_item *item) {
    Message m;
    m.subject    = utf8_field(item, &item->subject);
    m.plain_text = utf8_field(item, &item->body);
    m.comment    = utf8_field(item, &item->comment);
    if (item->email) {
        pst_item_email *e = item->email;
        m.sender      = utf8_field(item, &e->sender_address);
        m.sender_name = utf8_field(item, &e->outlook_sender_name);
        m.to          = utf8_field(item, &e->sentto_address);
        m.cc          = utf8_field(item, &e->cc_address);
        m.bcc         = utf8_field(item, &e->bcc_address);
        m.reply_to    = utf8_field(item, &e->reply_to);
        m.in_reply_to = utf8_field(item, &e->in_reply_to);
        m.message_id  = utf8_field(item, &e->messageid);
        m.header      = utf8_field(item, &e->header);
        m.html        = utf8_field(item, &e->htmlbody);
        m.rtf         = rtf_text(e->rtf_compressed);
        m.importance  = e->importance;
        m.priority    = e->priority;
        m.sensitivity = e->sensitivity;
        const FILETIME *when = e->arrival_date ? e->arrival_date : e->sent_date;
        m.date      = filetime_iso(when);
        m.sent_date = filetime_iso(e->sent_date);
    }
    for (pst_item_attach *a = item->attach; a; a = a->next) {
        m.attachments.push_back(extract_attachment(pf, item, a));
    }
    return m;
}

Contact extract_contact(pst_item *item) {
    Contact c;
    c.display_name = utf8_field(item, &item->file_as);
    pst_item_contact *ct = item->contact;
    if (ct) {
        c.fullname         = utf8_field(item, &ct->fullname);
        c.first_name       = utf8_field(item, &ct->first_name);
        c.surname          = utf8_field(item, &ct->surname);
        c.nickname         = utf8_field(item, &ct->nickname);
        c.company_name     = utf8_field(item, &ct->company_name);
        c.job_title        = utf8_field(item, &ct->job_title);
        c.department       = utf8_field(item, &ct->department);
        c.email1           = utf8_field(item, &ct->address1);
        c.email2           = utf8_field(item, &ct->address2);
        c.email3           = utf8_field(item, &ct->address3);
        c.business_phone   = utf8_field(item, &ct->business_phone);
        c.home_phone       = utf8_field(item, &ct->home_phone);
        c.mobile_phone     = utf8_field(item, &ct->mobile_phone);
        c.business_address = utf8_field(item, &ct->business_address);
        c.home_address     = utf8_field(item, &ct->home_address);
    }
    return c;
}

Appointment extract_appointment(pst_item *item) {
    Appointment a;
    a.subject = utf8_field(item, &item->subject);
    a.body    = utf8_field(item, &item->body);
    pst_item_appointment *ap = item->appointment;
    if (ap) {
        a.location               = utf8_field(item, &ap->location);
        a.start                  = filetime_iso(ap->start);
        a.end                    = filetime_iso(ap->end);
        a.timezone               = utf8_field(item, &ap->timezonestring);
        a.all_day                = ap->all_day != 0;
        a.busy_status            = ap->showas;
        a.is_recurring           = ap->is_recurring != 0;
        a.recurrence_type        = ap->recurrence_type;
        a.recurrence_description = utf8_field(item, &ap->recurrence_description);
    }
    return a;
}

Journal extract_journal(pst_item *item) {
    Journal j;
    j.subject = utf8_field(item, &item->subject);
    pst_item_journal *jn = item->journal;
    if (jn) {
        j.type        = utf8_field(item, &jn->type);
        j.description = utf8_field(item, &jn->description);
        j.start       = filetime_iso(jn->start);
        j.end         = filetime_iso(jn->end);
    }
    return j;
}

void collect(pst_file *pf, pst_desc_tree *d_ptr, Folder &parent, int depth) {
    if (depth > kMaxFolderDepth) return;
    for (; d_ptr; d_ptr = d_ptr->next) {
        if (!d_ptr->desc) continue;
        pst_item *item = pst_parse_item(pf, d_ptr, NULL);
        if (!item) continue;

        if (item->folder) {
            // Recurse into every folder, even one with an empty display name,
            // so no branch of the tree is silently dropped.
            Folder f;
            f.name = utf8_field(item, &item->file_as);
            if (d_ptr->child) collect(pf, d_ptr->child, f, depth + 1);
            parent.subfolders.push_back(std::move(f));
        } else {
            switch (item->type) {
                case PST_TYPE_CONTACT:
                    if (item->contact) parent.contacts.push_back(extract_contact(item));
                    break;
                case PST_TYPE_APPOINTMENT:
                case PST_TYPE_SCHEDULE:
                    if (item->appointment) parent.appointments.push_back(extract_appointment(item));
                    break;
                case PST_TYPE_JOURNAL:
                    if (item->journal) parent.journals.push_back(extract_journal(item));
                    break;
                case PST_TYPE_NOTE:
                case PST_TYPE_REPORT:
                default:
                    if (item->email) parent.messages.push_back(extract_message(pf, item));
                    break;
            }
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
        .def_readonly("content_id", &Attachment::content_id)
        .def_readonly("size", &Attachment::size)
        .def_readonly("method", &Attachment::method)
        .def_readonly("position", &Attachment::position)
        .def_property_readonly("data", [](const Attachment &a) {
            return py::bytes(a.data);
        }, "Raw attachment bytes (may be empty for reference-only attachments).")
        .def("__repr__", [](const Attachment &a) {
            return "<Attachment filename='" + a.filename + "' size=" + std::to_string(a.size) + ">";
        });

    py::class_<Message>(m, "Message")
        .def_readonly("subject", &Message::subject)
        .def_readonly("sender", &Message::sender)
        .def_readonly("sender_name", &Message::sender_name)
        .def_readonly("to", &Message::to)
        .def_readonly("cc", &Message::cc)
        .def_readonly("bcc", &Message::bcc)
        .def_readonly("reply_to", &Message::reply_to)
        .def_readonly("in_reply_to", &Message::in_reply_to)
        .def_readonly("message_id", &Message::message_id)
        .def_readonly("date", &Message::date)
        .def_readonly("sent_date", &Message::sent_date)
        .def_readonly("header", &Message::header)
        .def_readonly("plain_text", &Message::plain_text)
        .def_readonly("html", &Message::html)
        .def_readonly("rtf", &Message::rtf)
        .def_readonly("comment", &Message::comment)
        .def_readonly("importance", &Message::importance)
        .def_readonly("priority", &Message::priority)
        .def_readonly("sensitivity", &Message::sensitivity)
        .def_readonly("attachments", &Message::attachments)
        .def_property_readonly("attachment_names", [](const Message &m) {
            std::vector<std::string> names;
            for (const auto &a : m.attachments) names.push_back(a.filename);
            return names;
        })
        .def("__repr__", [](const Message &m) {
            return "<Message subject='" + m.subject + "' sender='" + m.sender + "'>";
        });

    py::class_<Contact>(m, "Contact")
        .def_readonly("display_name", &Contact::display_name)
        .def_readonly("fullname", &Contact::fullname)
        .def_readonly("first_name", &Contact::first_name)
        .def_readonly("surname", &Contact::surname)
        .def_readonly("nickname", &Contact::nickname)
        .def_readonly("company_name", &Contact::company_name)
        .def_readonly("job_title", &Contact::job_title)
        .def_readonly("department", &Contact::department)
        .def_readonly("email1", &Contact::email1)
        .def_readonly("email2", &Contact::email2)
        .def_readonly("email3", &Contact::email3)
        .def_readonly("business_phone", &Contact::business_phone)
        .def_readonly("home_phone", &Contact::home_phone)
        .def_readonly("mobile_phone", &Contact::mobile_phone)
        .def_readonly("business_address", &Contact::business_address)
        .def_readonly("home_address", &Contact::home_address)
        .def("__repr__", [](const Contact &c) {
            return "<Contact '" + (c.fullname.empty() ? c.display_name : c.fullname) + "'>";
        });

    py::class_<Appointment>(m, "Appointment")
        .def_readonly("subject", &Appointment::subject)
        .def_readonly("location", &Appointment::location)
        .def_readonly("start", &Appointment::start)
        .def_readonly("end", &Appointment::end)
        .def_readonly("timezone", &Appointment::timezone)
        .def_readonly("body", &Appointment::body)
        .def_readonly("all_day", &Appointment::all_day)
        .def_readonly("busy_status", &Appointment::busy_status)
        .def_readonly("is_recurring", &Appointment::is_recurring)
        .def_readonly("recurrence_type", &Appointment::recurrence_type)
        .def_readonly("recurrence_description", &Appointment::recurrence_description)
        .def("__repr__", [](const Appointment &a) {
            return "<Appointment subject='" + a.subject + "' start='" + a.start + "'>";
        });

    py::class_<Journal>(m, "Journal")
        .def_readonly("subject", &Journal::subject)
        .def_readonly("type", &Journal::type)
        .def_readonly("description", &Journal::description)
        .def_readonly("start", &Journal::start)
        .def_readonly("end", &Journal::end)
        .def("__repr__", [](const Journal &j) {
            return "<Journal subject='" + j.subject + "'>";
        });

    py::class_<Folder>(m, "Folder")
        .def_readonly("name", &Folder::name)
        .def_readonly("subfolders", &Folder::subfolders)
        .def_readonly("messages", &Folder::messages)
        .def_readonly("contacts", &Folder::contacts)
        .def_readonly("appointments", &Folder::appointments)
        .def_readonly("journals", &Folder::journals)
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
