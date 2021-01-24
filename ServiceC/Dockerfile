FROM gcc:latest
COPY servicec.cc /usr/src/
COPY sha1.h /usr/src/
COPY sha1.cpp /usr/src/
COPY somesock.h /usr/src/
COPY sometools.h /usr/src/
COPY websocket_client.h /usr/src/
COPY lib_ux usr/src/lib
COPY inc_ux usr/src/inc
WORKDIR /usr/src
RUN gcc sha1.cpp servicec.cc -o servicec -Iinc -Llib -lmicrohttpd -lpthread -static -DTARGET_UX
FROM ubuntu
COPY --from=0 usr/src/servicec .
CMD exec ./servicec
