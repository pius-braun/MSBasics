FROM openjdk:15-alpine
RUN apk add binutils # for objcopy, needed by jlink
COPY src/ApiGateway.java .
COPY src/libs/*.jar .
RUN javac -cp amqp-client-5.7.1.jar:javax.json-1.1.4.jar ApiGateway.java
RUN jdeps -cp "./*" --ignore-missing-deps --print-module-deps ApiGateway.class > java.modules
RUN jlink --strip-debug --add-modules $(cat java.modules) --output /java

FROM alpine
COPY --from=0 /java /java
COPY --from=0 ApiGateway*.class .
COPY --from=0 *.jar .
COPY src/res/index.html .
COPY src/res/favicon.ico /
EXPOSE 8080
CMD exec /java/bin/java -cp .:amqp-client-5.7.1.jar:slf4j-simple-1.7.26.jar:slf4j-api-1.7.26.jar:javax.json-1.1.4.jar ApiGateway