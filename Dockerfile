FROM alpine:3.20 as build
MAINTAINER Benjamin Henrion <zoobab@gmail.com>
LABEL Description="Build a static minichlink for Linux (x64) and Windows (x64)"

RUN apk add make gcc musl-dev libusb-dev linux-headers eudev-dev mingw-w64-gcc gcc-arm-none-eabi
COPY . /root/src
WORKDIR /root/src/minichlink
ENV OS=Linux
RUN make
ENV OS=Windows_NT
RUN make

FROM scratch
COPY --from=build /root/src/minichlink/minichlink /minichlink
COPY --from=build /root/src/minichlink/minichlink.exe /minichlink.exe
