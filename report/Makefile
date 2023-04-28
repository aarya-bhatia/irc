main:
	pdflatex main.tex

format:
	latexindent main.tex > format.tex
	cp main.tex /tmp/main.tex
	cp format.tex main.tex
	rm format.tex

clean:
	rm -rf *.aux *.log *.pdf
