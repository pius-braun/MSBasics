FROM openjdk:15-alpine
RUN apk add binutils
COPY src/ServiceJava.java .
COPY src/libs/*.jar .
RUN javac -cp amqp-client-5.7.1.jar:javax.json-1.1.4.jar ServiceJava.java
RUN jdeps -cp "./*" --ignore-missing-deps --print-module-deps ServiceJava.class >java.modules
RUN jlink --strip-debug --add-modules $(cat java.modules) --output /java
FROM alpine
COPY --from=0 /java /java
COPY --from=0 ServiceJava*.class .
COPY --from=0 *.jar .
EXPOSE 8081
CMD exec /java/bin/java -cp .:amqp-client-5.7.1.jar:slf4j-simple-1.7.26.jar:slf4j-api-1.7.26.jar:javax.json-1.1.4.jar ServiceJava
