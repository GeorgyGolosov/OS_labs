all: prog1 prog2 libimpl1.so libimpl2.so clean

# Program 1 (линковка на этапе компиляции)
prog1: prog1.o libimpl1.so
	g++ -o prog1 prog1.o -L. -limpl1 -Wl,-rpath,.

prog1.o: prog1.cpp
	g++ -c prog1.cpp

# Program 2 (динамическая загрузка)
prog2: prog2.o
	g++ -o prog2 prog2.o -ldl

prog2.o: prog2.cpp
	g++ -c prog2.cpp

# Combined Library for Implementation 1
libimpl1.so: math_lib1.o convert_lib1.o
	g++ -shared -o libimpl1.so math_lib1.o convert_lib1.o

math_lib1.o: math_lib1.cpp
	g++ -c -fPIC math_lib1.cpp

convert_lib1.o: convert_lib1.cpp
	g++ -c -fPIC convert_lib1.cpp

# Combined Library for Implementation 2
libimpl2.so: math_lib2.o convert_lib2.o
	g++ -shared -o libimpl2.so math_lib2.o convert_lib2.o

math_lib2.o: math_lib2.cpp
	g++ -c -fPIC math_lib2.cpp

convert_lib2.o: convert_lib2.cpp
	g++ -c -fPIC convert_lib2.cpp

# Clean: удаление объектных файлов
clean:
	rm -f *.o
