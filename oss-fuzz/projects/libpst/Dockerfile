FROM gcr.io/oss-fuzz-base/base-builder

RUN apt-get update && apt-get install -y --no-install-recommends \
        make autoconf automake libtool autoconf-archive \
        gettext autopoint pkg-config zlib1g-dev libgsf-1-dev zip \
    && rm -rf /var/lib/apt/lists/*

RUN git clone --depth 1 https://github.com/mrkxlia/libpst-win64 libpst
WORKDIR libpst
# The build logic is shared with ClusterFuzzLite; copy it out of the clone so
# OSS-Fuzz's `compile` finds it at $SRC/build.sh.
RUN cp .clusterfuzzlite/build.sh $SRC/build.sh
