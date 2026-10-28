// Verify GHOSTFOX_NECKO_WORKER: drive the engine's internal fetch()/navigate
// path, which is the code that actually spawns transfer workers.
package main

import (
	"fmt"
	"log"
	"os"

	ghostfox "ghostfox/ghostfox"
)

func main() {
	shell := os.Getenv("RUYIJS_SHELL")
	ctx, err := ghostfox.New(ghostfox.WithShell(shell), ghostfox.WithTimeout(45e9))
	if err != nil {
		log.Fatal(err)
	}
	defer ctx.Close()

	if err := ctx.LoadPage("https://start.local/", "<html><body>start</body></html>"); err != nil {
		log.Fatal("load:", err)
	}
	if err := ctx.Navigate("https://httpbin.org/get"); err != nil {
		fmt.Println("navigate err:", err)
	} else {
		fmt.Println("navigate ok")
	}
	url, _ := ctx.Eval("location.href")
	title, _ := ctx.Eval("document.body ? document.body.innerText.slice(0,80) : 'NO BODY'")
	fmt.Println("url:", url)
	fmt.Println("body:", title)

	status, ferr := ctx.Eval(`fetch("https://httpbin.org/user-agent").then(r => r.status).catch(e => "ERR:" + e.message)`)
	fmt.Println("fetch status:", status, "err:", ferr)
	body, ferr := ctx.Eval(`fetch("https://httpbin.org/user-agent").then(r => r.text()).catch(e => "ERR:" + e.message)`)
	fmt.Println("fetch body:", body, "err:", ferr)
	fmt.Println("NECKO WORKER TEST DONE")
}
