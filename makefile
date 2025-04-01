deps=$(shell find . -name '*.adoc')
htmls=pages/*

all: $(htmls)

$(htmls): $(deps)
	asciidoctor -r asciidoctor-multipage -b multipage_html5 index.adoc -D pages/

clean:
	rm -f pages/*