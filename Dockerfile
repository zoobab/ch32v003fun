FROM alpine:3.20 as build
MAINTAINER Benjamin Henrion <zoobab@gmail.com>
LABEL Description="Build a static minichlink for Linux (x64) and Windows (x64)"

RUN apk add make gcc musl-dev libusb-dev linux-headers eudev-dev mingw-w64-gcc gcc-arm-none-eabi zip
COPY . /root/src
WORKDIR /root/src/minichlink

ENV OS=Linux
RUN make
RUN export OUTDIR="minichlink_linux_amd64" && mkdir -pv out/$OUTDIR && cp -v minichlink out/$OUTDIR/minichlink && cd out && zip -r $OUTDIR.zip $OUTDIR/

ENV OS=Windows_NT
RUN make
RUN export OUTDIR="minichlink_windows_amd64" && mkdir -pv out/$OUTDIR && cp -v minichlink.exe out/$OUTDIR/minichlink.exe && cd out && zip -r $OUTDIR.zip $OUTDIR/

FROM scratch
COPY --from=build /root/src/minichlink/out /
