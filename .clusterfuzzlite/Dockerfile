# ClusterFuzzLite / OSS-Fuzz builder image for libpst.
FROM gcr.io/oss-fuzz-base/base-builder

RUN apt-get update && apt-get install -y --no-install-recommends \
        make autoconf automake libtool autoconf-archive \
        gettext autopoint pkg-config zlib1g-dev libgsf-1-dev zip \
    && rm -rf /var/lib/apt/lists/*

COPY . $SRC/libpst
WORKDIR $SRC/libpst
COPY .clusterfuzzlite/build.sh $SRC/build.sh
