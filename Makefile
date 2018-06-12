a.out: main.cpp Service.cpp
	g++ -std=c++14 -g -lpthread -Wall -Werror $^

clean:
	rm *.out
