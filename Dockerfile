FROM alpine:3.14 AS build

RUN apk add --no-cache \
  git \
  cmake \
  make \
  g++

COPY . /src/turtlecoin

WORKDIR /src/turtlecoin

RUN git submodule update --init --recursive

RUN mkdir -p build && \
  cd build && \
  cmake .. && \
  make

FROM alpine:3.14
COPY --from=build /src/turtlecoin/build/TurtleCoinSeed /turtlecoin/TurtleCoinSeed
WORKDIR /turtlecoin
ENTRYPOINT ["/turtlecoin/TurtleCoinSeed"]
