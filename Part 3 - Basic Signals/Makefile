all:
	gcc -c include/src/reg.c -o include/obj/reg.o -g
	gcc -o main include/src/ghmshell.c include/obj/reg.o -g
clean:
	rm include/obj/reg.o
	rm main
