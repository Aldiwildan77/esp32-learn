package main

import (
	"context"
	"fmt"
	"log"
	"os"
	"os/signal"
	"strconv"
	"syscall"

	mqtt "github.com/mochi-mqtt/server/v2"
	"github.com/mochi-mqtt/server/v2/hooks/auth"
	"github.com/mochi-mqtt/server/v2/listeners"
	"github.com/mochi-mqtt/server/v2/packets"
)

func main() {
	ctx := context.Background()
	sc := make(chan os.Signal, 1)
	signal.Notify(sc, syscall.SIGINT, syscall.SIGTERM, os.Interrupt)

	mqttSrv := mqtt.New(&mqtt.Options{InlineClient: true})
	defer mqttSrv.Close()

	_ = mqttSrv.AddHook(new(auth.AllowHook), nil)

	port := 1883

	tcp := listeners.NewTCP("t1", fmt.Sprintf(":%d", port), nil)
	err := mqttSrv.AddListener(tcp)
	if err != nil {
		log.Fatalf("error add listener: %v", err)
	}

	mqttSrv.Log.InfoContext(ctx, fmt.Sprintf("starting server on :%d", 1883))

	go func() {
		err := mqttSrv.Serve()
		if err != nil {
			log.Fatalf("error: %v", err)
		}
	}()

	go func() {
		// _ = mqttSrv.Publish("direct/retained", []byte("retained message"), true, 0)

		callbackFn := func(cl *mqtt.Client, sub packets.Subscription, pk packets.Packet) {
			mqttSrv.Log.InfoContext(ctx, "inline client received message from subscription", "client", cl.ID, "subscriptionId", sub.Identifier, "topic", pk.TopicName, "payload", string(pk.Payload))

			tempResult := "normal"

			if temp := string(pk.Payload); temp != "" {
				tempFloat, _ := strconv.ParseFloat(temp, 64)
				if tempFloat > 30.50 {
					tempResult = "high"
				}
			}

			_ = mqttSrv.Publish("temperature/result", []byte(tempResult), true, 0)
		}

		mqttSrv.Log.InfoContext(ctx, "inline client subscribing")
		_ = mqttSrv.Subscribe("temperature/send", 1, callbackFn)
	}()

	<-sc
	mqttSrv.Log.WarnContext(ctx, "caught signal, stopping...")
}
