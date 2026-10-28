// ghostfox stdio-RPC smoke test. Engine from RUYIJS_SHELL (or WithShell
// fallback / beside-the-executable discovery).
package main

import (
	"fmt"
	"log"
	"os"

	ghostfox "ghostfox/ghostfox"
)

func main() {
	ctx, err := ghostfox.New(
		ghostfox.WithShell(os.Getenv("RUYIJS_SHELL")),
		ghostfox.WithTimeout(30e9),
	)
	if err != nil {
		log.Fatal(err)
	}
	defer ctx.Close()

	ua, err := ctx.Eval("navigator.userAgent")
	fmt.Println("userAgent:", ua, "err:", err)

	brand, _ := ctx.Eval("Object.prototype.toString.call(navigator)")
	fmt.Println("brand:", brand)

	r, err := ctx.Eval("({ua: navigator.userAgent, langs: navigator.languages, dpr: devicePixelRatio, ok: 1+1})")
	fmt.Printf("composite: %#v err: %v\n", r, err)

	if err := ctx.LoadPage("https://gotest.local/x.html",
		"<!DOCTYPE html><html><head><title>go</title></head><body><b>hi</b></body></html>"); err != nil {
		fmt.Println("load_page err:", err)
	}
	title, _ := ctx.Eval("document.title + '|' + document.body.innerText + '|' + location.href")
	fmt.Println("page:", title)
	fmt.Println("GHOSTFOX SMOKE OK")
}
