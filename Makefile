TARGET=presat
${TARGET}.exe : ${TARGET}.o xmlvalues.o
	gcc -O3 ${TARGET}.o xmlvalues.o -o ${TARGET} -L/usr/local/lib -lsimu1 `xml2-config --libs`

${TARGET}.o : ${TARGET}.c xmlvalues.h
	gcc -O3 -c ${TARGET}.c

xmlvalues.o : xmlvalues.c
	gcc -O3 -c `xml2-config --cflags` xmlvalues.c

clean :
	rm -f *.o ${TARGET} ${TARGET}.exe
