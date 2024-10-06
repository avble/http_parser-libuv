all:
	g++ main.cpp -luv -lhttp_parser -o http_srv
clean:
	rm -f http_srv