package main

import (
	"github.com/pkg/sftp"
	"golang.org/x/crypto/ssh"
	"io"
	"log"
	"os"
	"strings"
	"bufio"
	"fmt"
	"path/filepath"
)

func main() {
	user := os.Getenv("REMOTE_USER");
	pass := os.Getenv("REMOTE_PASS");
	remote := os.Getenv("REMOTE_IP");
	file := os.Getenv("FILE_NAME");

	config := &ssh.ClientConfig{
		User: user,
		Auth: []ssh.AuthMethod{
			ssh.Password(pass),
		},
		HostKeyCallback: ssh.InsecureIgnoreHostKey(),
	}

	// Connect.
	conn, err := ssh.Dial("tcp", remote + ":22", config)
	if err != nil {
		log.Fatal(err)
	}
	defer conn.Close()

	// Create new SFTP client.
	client, err := sftp.NewClient(conn)
	if err != nil {
		log.Fatal(err)
	}
	defer client.Close()

 	remotePath := os.Getenv("REMOTE_PATH") + file;
 	client.Remove(remotePath);
	// Create destination file.
	dstFile, err := client.Create(remotePath)
	if err != nil {
		log.Fatal(err)
	}
	defer dstFile.Close()

	// Create source file.
	srcFile, err := os.Open("./" + file)
	if err != nil {
		log.Fatal(err)
	}

	// Copy source file to destination file.
	bytes, err := io.Copy(dstFile, srcFile)
	if err != nil {
		log.Fatal(err)
	}
	fmt.Printf("%d bytes copied\n", bytes)
}

func getHostKey(host string) ssh.PublicKey {
	// Parse OpenSSH known_hosts file.
	// SSH or use ssh-keyscan to get initial key.
	file, err := os.Open(filepath.Join(os.Getenv("HOME"), ".ssh", "known_hosts"))
	if err != nil {
		log.Fatal(err)
	}
	defer file.Close()

	scanner := bufio.NewScanner(file)
	var hostKey ssh.PublicKey
	for scanner.Scan() {
		fields := strings.Split(scanner.Text(), " ")
		if len(fields) != 3 {
			continue
		}
		if strings.Contains(fields[0], host) {
			var err error
			hostKey, _, _, _, err = ssh.ParseAuthorizedKey(scanner.Bytes())
			if err != nil {
				log.Fatalf("error parsing %q: %v", fields[2], err)
			}
			break
		}
	}

	if hostKey == nil {
		log.Fatalf("no hostkey found for %s", host)
	}

	return hostKey
}
