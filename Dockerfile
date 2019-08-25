FROM alpine
WORKDIR /opt/aplus

COPY . .
RUN ./extra/utils/build-with-docker i686

CMD ["build", "run"]
