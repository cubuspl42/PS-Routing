a.out: main.cpp Service.cpp
	g++ -std=c++14 -Wall -Werror $^

clean:
	rm *.out
