package main

/*
#cgo CFLAGS: -I./record
#cgo LDFLAGS: -L. -lflacrecorder
#include <flacrecorder.h>
*/
import "C"
import (
	"os"
	"fmt"
	"unsafe"
	"context"
  "io/ioutil"
  "log"

	speech "cloud.google.com/go/speech/apiv1"
  speechpb "google.golang.org/genproto/googleapis/cloud/speech/v1"
)
//                        SampleRateHertz: 16000,
func main() {
	name := C.CString("record_file.flac")
	defer C.free(unsafe.Pointer(name))
	C.record(name)
	ctx := context.Background()
        client, err := speech.NewClient(ctx)
        if err != nil {
                log.Fatalf("Failed to create client: %v", err)
        }
				dir, _ := os.Getwd()
        filename := dir + "/record_file.flac"

        data, err := ioutil.ReadFile(filename)
        if err != nil {
                log.Fatalf("Failed to read file: %v", err)
        }

        resp, err := client.Recognize(ctx, &speechpb.RecognizeRequest{
                Config: &speechpb.RecognitionConfig{
                        Encoding:        speechpb.RecognitionConfig_FLAC,
                        LanguageCode:    "en-US",
                },
                Audio: &speechpb.RecognitionAudio{
                        AudioSource: &speechpb.RecognitionAudio_Content{Content: data},
                },
        })
        if err != nil {
                log.Fatalf("failed to recognize: %v", err)
				}
				
        for _, result := range resp.Results {
                for _, alt := range result.Alternatives {
                        fmt.Printf("\"%v\" (confidence=%3f)\n", alt.Transcript, alt.Confidence)
                }
        }
}
