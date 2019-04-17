# PUBSUB

## Publish and Subscribe Overview

Pubsub utilizes a Publish/Subscribe paradigm model for real-time data streaming between several decoupled components in a application by using string topics separated by ".".

For example, a instances can publish it's temperature value in <instance_name>.evt.temp topic. This is useful when several other instances uses another instance data to process (for example one can make push the data to a server, another one can push it using Bluetooth or store it and calculate the median temperature). The instances subscribe to that topic and waits for the data to be received.

Other useful way modules use the pubsub system is when they offer an API to other modules or to other software. For example, the temperature module offers the topic <instance_name>.cmd.set.scale on the pubsub to send "ºC" or "ºF".

In addition, some topics in the pubsub expect the publisher to provide a pubsub response topic to publish a response. For example, a component is can publish to topic to the temperature component using  <instance_name>.cmd.get.cfg, witch packs the configuration and publishes the result in the response topic the caller provided. The response can be flagged as an error too if something failed.

## Types of data

The types of data that can be published and received through the pubsub are:

* integer
* double
* bool
* pointer
* string
* byte array


## Usage examples

