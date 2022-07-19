all: editor

editor: myEditor.cpp
	g++ -o editor myEditor.cpp -Wall -W;