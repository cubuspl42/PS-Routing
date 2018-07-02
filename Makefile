a.out: main.cpp Service.cpp NetlinkRouteSocket.cpp
	g++ -std=c++14 -g -lpthread -Wall -Werror $^

clean:
	rm *.out
