myshell: interpreter.o command.o
	g++ -o myshell interpreter.o command.o
interpreter.o: interpreter.cc command.h command.cc
	g++ -c interpreter.cc -o interpreter.o
command.o: command.cc command.h
	g++ -c command.cc -o command.o
clean:
	rm -f  *.o
