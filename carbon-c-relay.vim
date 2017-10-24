" Vim syntax file
" Language: carbon-c-relay config
" Maintainer: Fabian Groffen
" Latest Revision: 23 October 2017

if exists("b:current_syntax")
	finish
endif

syn match comment "#.*$"
syn region string start='"' end='"'
syn match int '\<\d\+\>' contained
syn match star '\<\*\>' contained

" Keywords
syn keyword startKeywords cluster match rewrite aggregate statistics listen include contained
syn keyword clusterKeywords forward any_of failover carbon_ch fnv1a_ch jump_fnv1a_ch file ip replication proto useall udp tcp type linemode transport gzip lz4 ssl contained
syn keyword matchKeywords validate else log drop route using send to blackhole stop contained
syn keyword rewriteKeywords into contained
syn keyword aggregateKeywords every seconds expire after timestamp at start middle end of bucket compute sum count cnt maximum max minimum min average avg median variance stddev percentile write to send stop contained
syn match percentile '\<percentile\d+\>' contained
syn keyword statisticsKeywords submit every seconds reset counters after interval prefix with send to stop contained
syn keyword listenKeywords type linemode transport gzip lz4 ssl proto udp tcp unix contained
"syn keyword includeKeywords contained

" Regions
syn region cluster start="cluster" end=";" fold transparent contains=startKeywords,clusterKeywords,string,int,comment
syn region match start="match" end=";" fold transparent contains=startKeywords,matchKeywords,star,string,comment
syn region rewrite start="rewrite" end=";" fold transparent contains=startKeywords,rewriteKeywords,string,comment
syn region match start="aggregate" end=";" fold transparent contains=startKeywords,aggregateKeywords,percentile,string,int,comment
syn region match start="statistics" end=";" fold transparent contains=startKeywords,statisticsKeywords,string,int,comment
syn region match start="listen" end=";" fold transparent contains=startKeywords,listenKeywords,string,int,comment
syn region match start="include" end=";" fold transparent contains=startKeywords,string,comment

" Colouring
hi def link comment            Comment
hi def link string             Constant
hi def link int                Constant
hi def link star               Constant
hi def link startKeywords      Statement
hi def link clusterKeywords    PreProc
hi def link matchKeywords      PreProc
hi def link rewriteKeywords    PreProc
hi def link aggregateKeywords  PreProc
hi def link statisticsKeywords PreProc
hi def link listenKeywords     PreProc
"hi def link includeKeywords    PreProc
hi def link clName             Identifier

let b:current_syntax = "carbon-c-relay"
