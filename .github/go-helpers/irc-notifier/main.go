package main

import (
	"fmt"
	"time"
	"strings"
	"os"

	"github.com/gempir/go-twitch-irc"
)

var (
	client *twitch.Client;
)

func main() {
	user := os.Getenv("IRC_USER");
	pass := os.Getenv("IRC_PASS");
	ircMsg := os.Getenv("IRC_MESSAGE");
	ircChannel := os.Getenv("IRC_CHANNEL");
	commit := os.Getenv("COMMIT_MESSAGE");

	fmt.Println("COMMIT_MESSAGE:", commit);
	if (strings.Contains(commit, "Test") || len(commit) < 2) {
		fmt.Println("This is the test commit.");
		os.Exit(0);
	}

	client = twitch.NewClient(user, "oauth:" + pass);

	timer1 := time.NewTimer(5 * time.Second)

	client.Join(ircChannel);
	message := fmt.Sprintf(
		"Updated on %s GMT. ",
		time.Now().Format("02-01-2006 15:04:05")) + ircMsg;
	client.Say(ircChannel, message);

	client.OnConnect(func() {
		<-timer1.C
    	os.Exit(0);
	});

	err := client.Connect();
	if err == nil {
		panic(err);
	}
}
