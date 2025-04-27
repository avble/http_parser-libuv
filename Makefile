all:
	g++ http_parser.cpp main.cpp -luv -std=c++17 -o http_srv
clean:
	rm -f http_srv