// ghostfox stdio-RPC smoke test.
package main

import (
	"fmt"
	"log"

	ghostfox "ghostfox/ghostfox"
)

func main() {
	ctx, err := ghostfox.New(
		ghostfox.WithShell(`E:\ruyi-clone\firefox-158.0\obj-x86_64-pc-windows-msvc\dist\bin\js.exe`),
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
