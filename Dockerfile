FROM ruby:2.3.3
RUN apt-get update -qq && apt-get install -y build-essential
RUN mkdir /mygem
WORKDIR /mygem
ADD . /mygem
RUN bundle install

RUN apt-get install -y less
