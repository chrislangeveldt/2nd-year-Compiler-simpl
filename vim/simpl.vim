" Vim syntax file
" Language: SIMPL
" Maintainer: W. H. K. Bester
" Latest Revision: 23 August 2021

if exists("b:current_syntax")
	finish
endif

" keywords
syn keyword	simplBoolean			false true
syn keyword	simplCommand			read write
syn keyword	simplConditional		else elsif if
syn keyword	simplOperator			and not or mod
syn keyword	simplOperator			= # < > <= >= + - * / <- &
syn keyword	simplRepeat				while
syn keyword	simplBlockStatement		begin do end then
syn keyword	simplDefineStatement	define program ->
syn keyword	simplStatement			chill exit
syn keyword	simplType				array boolean integer

" literals
syn match	simplNumber				"-\?\d\+"
syn match	simplIdentifier			"\<[a-zA-Z_][a-zA-Z0-9_]*\>"
syn match	simplFunction			/\<[a-zA-Z_][a-zA-Z0-9_]*\s*(/me=e-1,he=e-1
syn region	simplString matchgroup=simplString start=+"+ skip=+\\.+ end=+"+

" comments
syn region	simplComment start="(\*" end="\*)" contains=simplTodo,simplComment
syn keyword	simplTodo contained		TODO FIXME XXX DEBUG NOTE HBD
syn match	simplTodo contained		"HERE BE DRAGONS"
syn match	simplTodo contained		"HIC SVNT DRACONES"
" Anyone who alleges a Harry Potter or Game of Thrones reference, will be
" eaten by said dragons.  Fieri potest ut cerebrum tuum liquefiat.  On the
" other hand: tugh qoH nachDaj je chevlu'ta'.

" associations
let b:current_syntax = "simpl"

" The following is a bit colourful, rather like what SublimeText fanboys are
" used to.  Do feel free to tune to taste.  (And if you are a SublimeText fan:
" I bite my thumb at you, and a plague on your house!)

hi def link	simplBlockStatement		Statement
hi def link	simplBoolean			Boolean
hi def link	simplCommand			Function
hi def link	simplComment			Comment
hi def link	simplConditional		Conditional
hi def link	simplDefineStatement	Statement
hi def link simplFunction			Function
" Uncommenting the following makes it *really* colourful.
" hi def link	simplIdentifier			Identifier
hi def link	simplNumber				Number
hi def link	simplRepeat				Repeat
hi def link	simplTodo				Todo
hi def link	simplType				Type
hi def link simplOperator			Operator
hi def link simplStatement			Keyword
hi def link simplString				String

" vim: ts=4 sw=2:
