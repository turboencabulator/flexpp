/* scan.l - scanner for flex input */

%{
/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Vern Paxson.
 *
 * The United States Government has rights in this work pursuant
 * to contract no. DE-AC03-76SF00098 between the United States
 * Department of Energy and the University of California.
 *
 * Redistribution and use in source and binary forms are permitted provided
 * that: (1) source distributions retain this entire copyright notice and
 * comment, and (2) distributions including binaries display the following
 * acknowledgement:  ``This product includes software developed by the
 * University of California, Berkeley and its contributors'' in the
 * documentation or other materials provided with the distribution and in
 * all advertising materials mentioning features or use of this software.
 * Neither the name of the University nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#undef yywrap

#include "flexdef.h"
#include "parse.h"

#define HEADER_ECHO fprintf(headerfile,"%s",yytext )
#define ACTION_ECHO fprintf( temp_action_file, "%s", yytext )
#define MARK_END_OF_PROLOG fprintf( temp_action_file, "%%%% end of prolog\n" );

#undef YY_DECL
#define YY_DECL \
	int flexscan()

#define RETURNCHAR \
	yylval = yytext[0]; \
	return ( CHAR );

#define RETURNNAME \
	(void) strcpy( nmstr, (char *) yytext ); \
	return ( NAME );

#define PUT_BACK_STRING(str, start) \
	for ( i = strlen( (char *) (str) ) - 1; i >= start; --i ) \
	    unput((str)[i])

#define CHECK_REJECT(str) \
	if ( all_upper( str ) ) \
	    reject = true;

#define CHECK_YYMORE(str) \
	if ( all_lower( str ) ) \
	    yymore_used = true;
%}

%x SECT2 SECT2PROLOG SECT3 CODEBLOCK PICKUPDEF SC CARETISBOL NUM QUOTE
%x FIRSTCCL CCL ACTION RECOVER BRACEERROR C_COMMENT ACTION_COMMENT
%x ACTION_STRING PERCENT_BRACE_ACTION USED_LIST CODEBLOCK_2 XLATION
%x HEADER_BLOC HEADER2_BLOC NAME_DECLARE DEFINE_DECLARE DEFINE_CONTENT

WS		[ \t\f]+
OPTWS		[ \t\f]*
NOT_WS		[^ \t\f\n\r]

NAME		[a-z_][a-z_0-9-]*
NOT_NAME	[^a-z_\n\r]+

SCNAME		{NAME}
LINEFEED	("\n"|"\r"|"\r\n")
ESCSEQ		\\([^\n\r]|[0-9]{1,3}|x[0-9a-f]{1,2})

%%
    static int bracelevel, didadef;
    int i, indented_code, checking_used, new_xlation;
    int doing_codeblock = false;
    Char nmdef[MAXLINE], myesc();

^{WS}			indented_code = true; BEGIN(CODEBLOCK);
^#.*{LINEFEED}			++linenum; /* treat as a comment */
^"//".*{LINEFEED}		++linenum;  /* treat as a c++ comment */
^"/*"			ECHO; BEGIN(C_COMMENT);
^"%s"{NAME}?		return ( SCDECL );
^"%x"{NAME}?		return ( XSCDECL );
^"%name"{WS}		BEGIN(NAME_DECLARE);
^"%define"{WS}		BEGIN(DEFINE_DECLARE);


^"%{".*{LINEFEED}		{
			++linenum;
			line_directive_out( stdout );
			indented_code = false;
			BEGIN(CODEBLOCK);
			}
^"%header{".*{LINEFEED}		{
			++linenum;
			line_directive_out( headerfile );
			BEGIN(HEADER_BLOC);
			}

{WS}			return ( WHITESPACE );

^"%%".*			{
			set_lexer_name((char *)0);
			sectnum = 2;
			line_directive_out( headerfile );
      			if(headerfilename!=NULL)
		        {
      			 fprintf(stdout,
                          "#include \"%s\"\n",includefilename);
 			 header_skeleton_out();
    		 	};
			line_directive_out( stdout );
			BEGIN(SECT2PROLOG);
			return ( SECTEND );
			}

^"%used"		{
	pinpoint_message( "warning - %%used/%%unused have been deprecated" );
			checking_used = REALLY_USED; BEGIN(USED_LIST);
			}
^"%unused"		{
			checking_used = REALLY_NOT_USED; BEGIN(USED_LIST);
	pinpoint_message( "warning - %%used/%%unused have been deprecated" );
			checking_used = REALLY_NOT_USED; BEGIN(USED_LIST);
			}


^"%"[aeknopt]{WS}.*{LINEFEED}	{
#ifdef NOTDEF
			fprintf( stderr,
			     "old-style lex command at line %d ignored:\n\t%s",
				 linenum, yytext );
#endif
			++linenum;
			}

^"%"[cr]{OPTWS}		/* ignore old lex directive */

%t{OPTWS}{LINEFEED}		{
			++linenum;
			xlation =
			    (int *) malloc( sizeof( int ) * (unsigned) csize );

			if ( ! xlation )
			    flexfatal(
				"dynamic memory failure building %t table" );

			for ( i = 0; i < csize; ++i )
			    xlation[i] = 0;

			num_xlations = 0;

			BEGIN(XLATION);
			}

^"%"[^sxanpekotcru{}]{OPTWS}	synerr( "unrecognized '%' directive" );

^{NAME}			{
			(void) strcpy( nmstr, (char *) yytext );
			didadef = false;
			BEGIN(PICKUPDEF);
			}

{SCNAME}		RETURNNAME;
^{OPTWS}{LINEFEED}		++linenum; /* allows blank lines in section 1 */
{OPTWS}{LINEFEED}		++linenum; return ( '\n' );
.			{synerr( "illegal character" );
                         fprintf(stderr,
                           "Char : \\0x%x\n",yytext[yyleng-1]);
			BEGIN(RECOVER);}

<NAME_DECLARE>{NAME}	{set_lexer_name((char *)yytext);}
<NAME_DECLARE>{WS}	;
<NAME_DECLARE>{LINEFEED}	++linenum;BEGIN(INITIAL);
<NAME_DECLARE>.		synerr( "illegal character" );BEGIN(RECOVER);

<DEFINE_DECLARE>{NAME}	{ set_lexer_name((char *)0);
			line_directive_out( headerfile );
			fprintf(headerfile,"#define YY_%s_%s",lexer_name,yytext);
			 BEGIN(DEFINE_CONTENT);
			}

<DEFINE_DECLARE>.	synerr( "illegal character" ); BEGIN(RECOVER);
<DEFINE_DECLARE>{LINEFEED}	++linenum;BEGIN(INITIAL);

<DEFINE_CONTENT>\\{LINEFEED}	HEADER_ECHO;++linenum;
<DEFINE_CONTENT>{LINEFEED}	++linenum;HEADER_ECHO;BEGIN(INITIAL);
<DEFINE_CONTENT>.	HEADER_ECHO;

<C_COMMENT>"*/"		ECHO; BEGIN(INITIAL);
<C_COMMENT>"*/".*{LINEFEED}	++linenum; ECHO; BEGIN(INITIAL);
<C_COMMENT>[^*\n\r]+	ECHO;
<C_COMMENT>"*"		ECHO;
<C_COMMENT>{LINEFEED}		++linenum; ECHO;

<HEADER_BLOC>^"%}".*{LINEFEED}	++linenum; BEGIN(INITIAL);
<HEADER_BLOC>"reject"	HEADER_ECHO; CHECK_REJECT(yytext);
<HEADER_BLOC>"yymore"	HEADER_ECHO; CHECK_YYMORE(yytext);
<HEADER_BLOC>{NAME}|{NOT_NAME}|.	HEADER_ECHO;
<HEADER_BLOC>{LINEFEED}		{
			++linenum;
			HEADER_ECHO;
			}


<HEADER2_BLOC>^"%}".*{LINEFEED}	++linenum; BEGIN(SECT2);
<HEADER2_BLOC>"reject"	HEADER_ECHO; CHECK_REJECT(yytext);
<HEADER2_BLOC>"yymore"	HEADER_ECHO; CHECK_YYMORE(yytext);
<HEADER2_BLOC>{NAME}|{NOT_NAME}|.	HEADER_ECHO;
<HEADER2_BLOC>{LINEFEED}		{
			++linenum;
			HEADER_ECHO;
			}




<CODEBLOCK>^"%}".*{LINEFEED}	++linenum; BEGIN(INITIAL);
<CODEBLOCK>"reject"	ECHO; CHECK_REJECT(yytext);
<CODEBLOCK>"yymore"	ECHO; CHECK_YYMORE(yytext);
<CODEBLOCK>{NAME}|{NOT_NAME}|.	ECHO;
<CODEBLOCK>{LINEFEED}		{
			++linenum;
			ECHO;
			if ( indented_code )
			    BEGIN(INITIAL);
			}


<PICKUPDEF>{WS}		/* separates name and definition */

<PICKUPDEF>{NOT_WS}.*	{
			(void) strcpy( (char *) nmdef, (char *) yytext );

			for ( i = strlen( (char *) nmdef ) - 1;
			      i >= 0 &&
			      nmdef[i] == ' ' || nmdef[i] == '\t';
			      --i )
			    ;

			nmdef[i + 1] = '\0';

                        ndinstal( nmstr, nmdef );
			didadef = true;
			}

<PICKUPDEF>{LINEFEED}		{
			if ( ! didadef )
			    synerr( "incomplete name definition" );
			BEGIN(INITIAL);
			++linenum;
			}

<RECOVER>.*{LINEFEED}		++linenum; BEGIN(INITIAL); RETURNNAME;


<USED_LIST>{LINEFEED}		++linenum; BEGIN(INITIAL);
<USED_LIST>{WS}
<USED_LIST>"reject"	{
			if ( all_upper( yytext ) )
			    reject_really_used = checking_used;
			else
			    synerr( "unrecognized %used/%unused construct" );
			}
<USED_LIST>"yymore"	{
			if ( all_lower( yytext ) )
			    yymore_really_used = checking_used;
			else
			    synerr( "unrecognized %used/%unused construct" );
			}
<USED_LIST>{NOT_WS}+	synerr( "unrecognized %used/%unused construct" );


<XLATION>"%t"{OPTWS}{LINEFEED}	++linenum; BEGIN(INITIAL);
<XLATION>^{OPTWS}[0-9]+	++num_xlations; new_xlation = true;
<XLATION>^.		synerr( "bad row in translation table" );
<XLATION>{WS}		/* ignore whitespace */

<XLATION>{ESCSEQ}	{
			xlation[myesc( yytext )] =
				(new_xlation ? num_xlations : -num_xlations);
			new_xlation = false;
			}
<XLATION>.		{
			xlation[yytext[0]] =
				(new_xlation ? num_xlations : -num_xlations);
			new_xlation = false;
			}

<XLATION>{LINEFEED}		++linenum;


<SECT2PROLOG>.*{LINEFEED}/{NOT_WS}	{
			++linenum;
			ACTION_ECHO;
			MARK_END_OF_PROLOG;
			BEGIN(SECT2);
			}

<SECT2PROLOG>.*{LINEFEED}	++linenum; ACTION_ECHO;

<SECT2PROLOG><<EOF>>	MARK_END_OF_PROLOG; yyterminate();

<SECT2>^{OPTWS}{LINEFEED}	++linenum; /* allow blank lines in section 2 */

<SECT2>^"%header{".*{LINEFEED}	{
			line_directive_out( headerfile );
			BEGIN(HEADER2_BLOC);
			}
<SECT2>^({WS}|"%{")	{
			indented_code = (yytext[0] != '%');
			doing_codeblock = true;
			bracelevel = 1;

			if ( indented_code )
			    ACTION_ECHO;

			BEGIN(CODEBLOCK_2);
			}

<SECT2>"<"		BEGIN(SC); return ( '<' );
<SECT2>^"^"		return ( '^' );
<SECT2>\"		BEGIN(QUOTE); return ( '"' );
<SECT2>"{"/[0-9]		BEGIN(NUM); return ( '{' );
<SECT2>"{"[^0-9\n\r][^}\n\r]*	BEGIN(BRACEERROR);
<SECT2>"$"/[ \t\n\r]	return ( '$' );

<SECT2>{WS}"%{"		{
			bracelevel = 1;
			BEGIN(PERCENT_BRACE_ACTION);
			return ( '\n' );
			}
<SECT2>{WS}"|".*{LINEFEED}	continued_action = true; ++linenum; return ( '\n' );

<SECT2>{WS}		{
			/* this rule is separate from the one below because
			 * otherwise we get variable trailing context, so
			 * we can't build the scanner using -{f,F}
			 */
			bracelevel = 0;
			continued_action = false;
			BEGIN(ACTION);
			return ( '\n' );
			}

<SECT2>{OPTWS}/{LINEFEED}	{
			bracelevel = 0;
			continued_action = false;
			BEGIN(ACTION);
			return ( '\n' );
			}

<SECT2>^{OPTWS}{LINEFEED}	++linenum; return ( '\n' );

<SECT2>"<<EOF>>"	return ( EOF_OP );

<SECT2>^"%%".*		{
			sectnum = 3;
			BEGIN(SECT3);
			return ( EOF ); /* to stop the parser */
			}

<SECT2>"["([^\\\]\n\r]|{ESCSEQ})+"]"	{
			int cclval;

			(void) strcpy( nmstr, (char *) yytext );

			/* check to see if we've already encountered this ccl */
			if ( (cclval = ccllookup( (Char *) nmstr )) )
			    {
			    yylval = cclval;
			    ++cclreuse;
			    return ( PREVCCL );
			    }
			else
			    {
			    /* we fudge a bit.  We know that this ccl will
			     * soon be numbered as lastccl + 1 by cclinit
			     */
			    cclinstal( (Char *) nmstr, lastccl + 1 );

			    /* push back everything but the leading bracket
			     * so the ccl can be rescanned
			     */
			    PUT_BACK_STRING((Char *) nmstr, 1);

			    BEGIN(FIRSTCCL);
			    return ( '[' );
			    }
			}

<SECT2>"{"{NAME}"}"	{
			register Char *nmdefptr;
			Char *ndlookup();

			(void) strcpy( nmstr, (char *) yytext );
			nmstr[yyleng - 1] = '\0';  /* chop trailing brace */

			/* lookup from "nmstr + 1" to chop leading brace */
			if ( ! (nmdefptr = ndlookup( nmstr + 1 )) )
			    synerr( "undefined {name}" );

			else
			    { /* push back name surrounded by ()'s */
			    unput(')');
			    PUT_BACK_STRING(nmdefptr, 0);
			    unput('(');
			    }
			}

<SECT2>[/|*+?.()]	return ( yytext[0] );
<SECT2>.		RETURNCHAR;
<SECT2>{LINEFEED}		++linenum; return ( '\n' );


<SC>","			return ( ',' );
<SC>">"			BEGIN(SECT2); return ( '>' );
<SC>">"/"^"		BEGIN(CARETISBOL); return ( '>' );
<SC>{SCNAME}		RETURNNAME;
<SC>.			synerr( "bad start condition name" );

<CARETISBOL>"^"		BEGIN(SECT2); return ( '^' );


<QUOTE>[^"\n\r]		RETURNCHAR;
<QUOTE>\"		BEGIN(SECT2); return ( '"' );

<QUOTE>{LINEFEED}		{
			synerr( "missing quote" );
			BEGIN(SECT2);
			++linenum;
			return ( '"' );
			}


<FIRSTCCL>"^"/[^-\n\r]	BEGIN(CCL); return ( '^' );
<FIRSTCCL>"^"/-		return ( '^' );
<FIRSTCCL>-		BEGIN(CCL); yylval = '-'; return ( CHAR );
<FIRSTCCL>.		BEGIN(CCL); RETURNCHAR;

<CCL>-/[^\]\n\r]		return ( '-' );
<CCL>[^\]\n\r]		RETURNCHAR;
<CCL>"]"		BEGIN(SECT2); return ( ']' );


<NUM>[0-9]+		{
			yylval = myctoi( yytext );
			return ( NUMBER );
			}

<NUM>","			return ( ',' );
<NUM>"}"			BEGIN(SECT2); return ( '}' );

<NUM>.			{
			synerr( "bad character inside {}'s" );
			BEGIN(SECT2);
			return ( '}' );
			}

<NUM>{LINEFEED}			{
			synerr( "missing }" );
			BEGIN(SECT2);
			++linenum;
			return ( '}' );
			}


<BRACEERROR>"}"		synerr( "bad name in {}'s" ); BEGIN(SECT2);
<BRACEERROR>{LINEFEED}		synerr( "missing }" ); ++linenum; BEGIN(SECT2);


<PERCENT_BRACE_ACTION,CODEBLOCK_2>{OPTWS}"%}".*		bracelevel = 0;
<PERCENT_BRACE_ACTION,CODEBLOCK_2,ACTION>"reject"	{
			ACTION_ECHO;
			CHECK_REJECT(yytext);
			}
<PERCENT_BRACE_ACTION,CODEBLOCK_2,ACTION>"yymore"	{
			ACTION_ECHO;
			CHECK_YYMORE(yytext);
			}
<PERCENT_BRACE_ACTION,CODEBLOCK_2>{NAME}|{NOT_NAME}|.	ACTION_ECHO;
<PERCENT_BRACE_ACTION,CODEBLOCK_2>{LINEFEED}			{
			++linenum;
			ACTION_ECHO;
			if ( bracelevel == 0 ||
			     (doing_codeblock && indented_code) )
			    {
			    if ( ! doing_codeblock )
				fputs( "\tYY_BREAK\n", temp_action_file );

			    doing_codeblock = false;
			    BEGIN(SECT2);
			    }
			}


	/* Reject and YYmore() are checked for above, in PERCENT_BRACE_ACTION */
<ACTION>"{"		ACTION_ECHO; ++bracelevel;
<ACTION>"}"		ACTION_ECHO; --bracelevel;
<ACTION>[^a-z_{}"'/\n\r]+	ACTION_ECHO;
<ACTION>{NAME}		ACTION_ECHO;
<ACTION>"//".*{LINEFEED}	++linenum;ACTION_ECHO;
<ACTION>"/*"		ACTION_ECHO; BEGIN(ACTION_COMMENT);
<ACTION>"'"([^'\\\n\r]|\\.)*"'"	ACTION_ECHO; /* character constant */
<ACTION>\"		ACTION_ECHO; BEGIN(ACTION_STRING);
<ACTION>{LINEFEED}		{
			++linenum;
			ACTION_ECHO;
			if ( bracelevel == 0 )
			    {
			    fputs( "\tYY_BREAK\n", temp_action_file );
			    BEGIN(SECT2);
			    }
			}
<ACTION>.		ACTION_ECHO;

<ACTION_COMMENT>"*/"	ACTION_ECHO; BEGIN(ACTION);
<ACTION_COMMENT>[^*\n\r]+	ACTION_ECHO;
<ACTION_COMMENT>"*"	ACTION_ECHO;
<ACTION_COMMENT>{LINEFEED}	++linenum; ACTION_ECHO;
<ACTION_COMMENT>.	ACTION_ECHO;

<ACTION_STRING>[^"\\\n\r]+	ACTION_ECHO;
<ACTION_STRING>\\.	ACTION_ECHO;
<ACTION_STRING>{LINEFEED}	++linenum; ACTION_ECHO;
<ACTION_STRING>\"	ACTION_ECHO; BEGIN(ACTION);
<ACTION_STRING>.	ACTION_ECHO;

<ACTION,ACTION_COMMENT,ACTION_STRING><<EOF>>	{
			synerr( "EOF encountered inside an action" );
			yyterminate();
			}


<SECT2,QUOTE,CCL>{ESCSEQ}	{
			yylval = myesc( yytext );
			return ( CHAR );
			}

<FIRSTCCL>{ESCSEQ}	{
			yylval = myesc( yytext );
			BEGIN(CCL);
			return ( CHAR );
			}


<SECT3>.*({LINEFEED}?)		ECHO;
%%


int yywrap()

    {
    fclose( yyin );
    if ( --num_input_files > 0 )
	{
	set_input_file( *++input_files );
	return ( 0 );
	}

    else
	return ( 1 );
    }


/* set_input_file - open the given file (if NULL, stdin) for scanning */

void set_input_file( file )
char *file;

    {
    if ( file )
	{
	infilename = file;
	yyin = fopen( infilename, "r" );

	if ( yyin == NULL )
	    lerrsf( "can't open %s", file );
	}

    else
	{
	yyin = stdin;
	infilename = "<stdin>";
	}
    }
