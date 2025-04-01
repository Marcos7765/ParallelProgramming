deps=$(shell find . -name '*.adoc')
htmls=docs/*

all: $(htmls)

$(htmls): $(deps)
	asciidoctor -r asciidoctor-multipage -b multipage_html5 index.adoc -D docs/

clean:
	rm -f docs/*