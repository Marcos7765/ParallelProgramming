deps=$(shell find . -name '*.adoc')
htmls=docs/*

all: $(htmls)

$(htmls): $(deps)
	asciidoctor -r asciidoctor-multipage -b multipage_html5 index.adoc -D docs/
	rm -rf docs/resources
	sh copy_resources.sh > /dev/null 2>&1

clean:
	rm -rf docs/*