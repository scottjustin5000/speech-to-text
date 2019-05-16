dynamic:
		rm -f libflacrecorder.dylib
		rm -f ../libflacrecorder.dylib
		gcc -c -fPIC flacrecorder.c
		gcc -dynamiclib -L/usr/local/Cellar/libsndfile/1.0.28/lib -lsndfile -L/usr/local/Cellar/portaudio/19.6.0/lib -lportaudio  -o libflacrecorder.dylib flacrecorder.o
		rm -f flacrecorder.o
		cp libflacrecorder.dylib ../libflacrecorder.dylib

shared:
		rm -f libflacrecorder.so
		rm -f ../libflacrecorder.so
		gcc -c -fPIC flacrecorder.c
		gcc -shared -W1 -lsndfile -lportaudio -o libflacrecorder.so flacrecorder.o
		rm -f flacrecorder.o
		cp libflacrecorder.so ../libflacrecorder.so