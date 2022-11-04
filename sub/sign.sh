#!/usr/bin/env bash

ASN="${1}"
SIGFILE="${2}"

case $ASN in
	scanner)
		FULLASN="Scanner"
		;;
	parser)
		FULLASN="Scanner \& Parser"
		;;
	symtable)
		FULLASN="Symbol Table \& Type Checking"
		;;
	codegen)
		FULLASN="Code Generation"
		;;
esac

if [ -z "${FULLASN}" ] || [ -z "${SIGFILE}" ]; then
	echo "usage: ${0} <scanner|parser|symtable|codegen> <path_to_sigfile>"
	exit 1
fi
if [ ! -f "${SIGFILE}" ]; then
	echo "'${SIGFILE}' could not be found or opened"
	exit 2
fi

TEXROOT="plagdecl-${ASN}"
TEXFILE="${TEXROOT}.tex"
AUXFILE="${TEXROOT}.aux"
LOGFILE="${TEXROOT}.log"

source personal.txt

cat > ${TEXFILE} <<__HERE__
\\documentclass[a4paper,11pt]{article}
\\usepackage[utf8]{inputenc}
\\usepackage{a4wide}
\\usepackage[T1]{fontenc}
\\usepackage{libertine}
\\usepackage{libertinust1math}
\\usepackage{microtype}
\\usepackage{graphicx}
\\usepackage{letterspace}
\\usepackage{tabularx}
\\usepackage{textcase}
\\DeclareTextFontCommand{\\textsmallcaps}{\\scshape}
\\newcommand{\\allcapsspacing}[1]{\\textls[200]{#1}}
\\newcommand{\\smallcapsspacing}[1]{\\textls[50]{#1}}
\\newcommand{\\allcaps}[1]{\\allcapsspacing{\\MakeTextUppercase{#1}}}
\\newcommand{\\smallcaps}[1]{\\smallcapsspacing{\\scshape\\MakeTextLowercase{#1}}}
\\renewcommand{\\textsc}[1]{\\smallcapsspacing{\\textsmallcaps{#1}}}
\\makeatletter
\\renewcommand{\\maketitle}{%
\\newpage
\\global\\@topnum\\z@% prevent floats from being placed at the top of the page
\\begingroup
\\centering
\\setlength{\\parindent}{0pt}%
\\setlength{\\parskip}{4pt}%
\\let\\@@title\\@empty
\\let\\@@author\\@empty
\\let\\@@date\\@empty
\\gdef\\@@title{\\sffamily\\LARGE\\textbf{\\allcaps{\\@title}}}%
\\gdef\\@@author{\\sffamily\\Large\\allcaps{\\@author}}%
\\gdef\\@@date{\\sffamily\\Large\\allcaps{\\@date}}%
\\@@author \\\\[1ex]
\\@@title \\\\[1ex]
\\@@date \\\\[1em]
\\endgroup
\\@afterindentfalse\\@afterheading% suppress indentation of the next paragraph
\\vspace{1em}
}
\\makeatletter
\\title{Practicum Plagiarism Declaration}
\\author{Computer Science 244}
\\date{Stellenbosch University, 2021}
\\setlength{\\parindent}{0pt}
\\pagestyle{empty}
\\begin{document}
\\maketitle
\\noindent\\includegraphics[width=\\textwidth]{plagdecl.pdf}
\\par\\vfill
\\begin{tabularx}{\\textwidth}{@{}l@{}p{1ex}@{}X@{}}
\\textsf{\\smallcaps{Assignment:}}\\enspace\\hrulefill &
\\hrulefill &
\\makebox[0pt][l]{\\raisebox{5pt}{CS244 Compiler Project (${FULLASN})}}\\hrulefill \\\\[1em]
\\textsf{\\smallcaps{Full names \\& surname:}}\\enspace\\hrulefill &
\\hrulefill &
\\makebox[0pt][l]{\\raisebox{5pt}{${FQNAME}}}\\hrulefill \\\\[1em]
\\textsf{\\smallcaps{Student number:}}\\enspace\\hrulefill &
\\hrulefill &
\\makebox[0pt][l]{\\raisebox{5pt}{${SUNUM}}}\\hrulefill
\\end{tabularx}
\\vspace{1em}
\\begin{tabularx}{\\textwidth}{@{}X@{\\enspace}X@{}}
\\textsf{\\smallcaps{Signature:}}\\enspace\\hrulefill\\makebox[0pt][c]{\\raisebox{5pt}{\\includegraphics[width=7em,height=7em,keepaspectratio]{${SIGFILE}}}}\\hrulefill &
  \\textsf{\\smallcaps{Date:}}\\enspace\\hrulefill\\makebox[0pt][c]{\\enspace\\raisebox{5pt}{$(date +"%-d %B %Y")}}\\hrulefill
\\end{tabularx}
\\end{document}
__HERE__

echo "Signing takes a couple of seconds..."

pdflatex -halt-on-error ${TEXFILE} >/dev/null \
	&& pdflatex -halt-on-error ${TEXFILE} >/dev/null \
	&& (echo "Signed successfully!" ; rm ${TEXFILE} ; rm ${AUXFILE} ; rm ${LOGFILE}) \
	|| echo "Signing failed."
