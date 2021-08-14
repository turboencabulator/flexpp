/* flex - tool to generate fast lexical analyzers */

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1990 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#include "flexdef.h"

static char flex_version[] = PACKAGE_STRING ", based on 2.3.8 and modified by coetmeur@icdc.fr for c++";

/* declare functions that have forward references */

void flexinit(int, char**);
void readin();
void set_up_initial_allocations();


/* these globals are all defined and commented in flexdef.h */
int printstats, syntaxerror, eofseen, ddebug, trace, spprdflt;
int interactive, caseins, useecs, fulltbl, usemecs;
int fullspd, gen_line_dirs, performance_report, backtrack_report, csize;
int yymore_used, reject, real_reject, continued_action;
int yymore_really_used, reject_really_used;
int datapos, dataline, linenum;
FILE *skelfile = NULL;
char *infilename = NULL;
char *headerfilename=NULL;
FILE *headerfile=NULL;
char *outputfilename=NULL;
char *includefilename=NULL;
char *skelheaderfilename=NULL;
FILE *skelheaderfile=NULL;
int onestate[ONE_STACK_SIZE], onesym[ONE_STACK_SIZE];
int onenext[ONE_STACK_SIZE], onedef[ONE_STACK_SIZE], onesp;
int current_mns, num_rules, current_max_rules, lastnfa;
int *firstst, *lastst, *finalst, *transchar, *trans1, *trans2;
int *accptnum, *assoc_rule, *state_type, *rule_type, *rule_linenum;
int current_state_type;
int variable_trailing_context_rules;
int numtemps, numprots, protprev[MSP], protnext[MSP], prottbl[MSP];
int protcomst[MSP], firstprot, lastprot, protsave[PROT_SAVE_SIZE];
int numecs, nextecm[CSIZE + 1], ecgroup[CSIZE + 1], nummecs, tecfwd[CSIZE + 1];
int tecbck[CSIZE + 1];
int *xlation = (int *) 0;
int num_xlations;
int lastsc, current_max_scs, *scset, *scbol, *scxclu, *sceof, *actvsc;
char **scname;
int current_max_dfa_size, current_max_xpairs;
int current_max_template_xpairs, current_max_dfas;
int lastdfa, *nxt, *chk, *tnxt;
int *base, *def, *nultrans, NUL_ec, tblend, firstfree, **dss, *dfasiz;
union dfaacc_union *dfaacc;
int *accsiz, *dhash, numas;
int numsnpairs, jambase, jamstate;
int lastccl, current_maxccls, *cclmap, *ccllen, *cclng, cclreuse;
int current_max_ccl_tbl_size;
Char *ccltbl;
char *starttime, *endtime, nmstr[MAXLINE];
int sectnum, nummt, hshcol, dfaeql, numeps, eps2, num_reallocs;
int tmpuses, totnst, peakpairs, numuniq, numdup, hshsave;
int num_backtracking, bol_needed;
FILE *temp_action_file;
FILE *backtrack_file;
int end_of_buffer_state;
char *action_file_name;
char **input_files;
int num_input_files;
char *program_name;
char lexer_name[256]="lex";
int name_defined=0;
char *skelname = NULL;

#ifndef SHORT_FILE_NAMES
static char *outfile = "lex.yy.c";
#else
static char *outfile = "lexyy.c";
#endif
static int outfile_created = 0;
static int use_stdout;
/* suffixes recognised to convert C/C++ outputfilename to headerfilename */
static char *c_suffixes[]=
   {".c",".cc",".cpp",".cxx",".C",".CPP",".CXX",".CC",(char *)0};


int main( argc, argv )
int argc;
char **argv;

    {
    flexinit( argc, argv );

    readin();

    if ( syntaxerror )
	flexend( 1 );

    if ( yymore_really_used == REALLY_USED )
	yymore_used = true;
    else if ( yymore_really_used == REALLY_NOT_USED )
	yymore_used = false;

    if ( reject_really_used == REALLY_USED )
	reject = true;
    else if ( reject_really_used == REALLY_NOT_USED )
	reject = false;

    if ( performance_report )
	{
	if ( interactive )
	    fprintf( stderr,
		     "-I (interactive) entails a minor performance penalty\n" );

	if ( yymore_used )
	    fprintf( stderr, "yymore() entails a minor performance penalty\n" );

	if ( reject )
	    fprintf( stderr, "REJECT entails a large performance penalty\n" );

	if ( variable_trailing_context_rules )
	    fprintf( stderr,
"Variable trailing context rules entail a large performance penalty\n" );
	}

    if ( reject )
	real_reject = true;

    if ( variable_trailing_context_rules )
	reject = true;

    if ( (fulltbl || fullspd) && reject )
	{
	if ( real_reject )
	    flexerror( "REJECT cannot be used with -f or -F" );
	else
	    flexerror(
	"variable trailing context rules cannot be used with -f or -F" );
	}

    ntod();

    /* generate the C state transition tables from the DFA */
    make_tables();

    /* note, flexend does not return.  It exits with its argument as status. */

    flexend( 0 );

    /*NOTREACHED*/
    }


/* flexend - terminate flex
 *
 * synopsis
 *    int status;
 *    flexend( status );
 *
 *    status is exit status.
 *
 * note
 *    This routine does not return.
 */

void flexend( status )
int status;

    {
    int tblsiz;
    char *flex_gettime();

    if ( skelfile != NULL )
	{
	if ( ferror( skelfile ) )
	    flexfatal( "error occurred when reading skeleton file" );

	if ( fclose( skelfile ) )
	    flexfatal( "error occurred when closing skeleton file" );
	}

    if ( skelheaderfile != NULL )
	{
	if ( ferror( skelheaderfile ) )
	    flexfatal( "error occurred when reading header skeleton file" );

	if ( fclose( skelheaderfile ) )
	    flexfatal( "error occurred when closing haeder skeleton file" );
	}

    if ( temp_action_file )
	{
	if ( ferror( temp_action_file ) )
	    flexfatal( "error occurred when reading/writing temporary action file" );

	if ( fclose( temp_action_file ) )
	    flexfatal( "error occurred when closing temporary action file" );

	if ( unlink( action_file_name ) )
	    flexfatal( "error occurred when deleting temporary action file" );
	}

    if ( headerfilename && headerfile )
	{
	if ( ferror( headerfile ) )
	    flexfatal( "error occurred when writing header file" );

	if ( fclose( headerfile ) )
	    flexfatal( "error occurred when closing header file" );

	if ( status != 0 && unlink( headerfilename ) )
	    flexfatal( "error occurred when deleting header file" );
	}

    if ( outfile_created )
	{
	if ( ferror( stdout ) )
	    flexfatal( "error occurred when writing output file" );

	if ( fclose( stdout ) )
	    flexfatal( "error occurred when closing output file" );

	if ( status != 0 && unlink( outfile ) )
	    flexfatal( "error occurred when deleting output file" );
	}

    if ( backtrack_report && backtrack_file )
	{
	if ( num_backtracking == 0 )
	    fprintf( backtrack_file, "No backtracking.\n" );
	else if ( fullspd || fulltbl )
	    fprintf( backtrack_file,
		     "%d backtracking (non-accepting) states.\n",
		     num_backtracking );
	else
	    fprintf( backtrack_file, "Compressed tables always backtrack.\n" );

	if ( ferror( backtrack_file ) )
	    flexfatal( "error occurred when writing backtracking file" );

	if ( fclose( backtrack_file ) )
	    flexfatal( "error occurred when closing backtracking file" );
	}

    if ( printstats )
	{
	endtime = flex_gettime();

	fprintf( stderr, "%s\n", flex_version );
	fprintf( stderr, "usage statistics:\n" );
	fprintf( stderr, "  started at %s, finished at %s\n",
		 starttime, endtime );

	fprintf( stderr, "  scanner options: -" );

	if ( backtrack_report )
	    putc( 'b', stderr );
	if ( ddebug )
	    putc( 'd', stderr );
	if ( interactive )
	    putc( 'I', stderr );
	if ( caseins )
	    putc( 'i', stderr );
	if ( ! gen_line_dirs )
	    putc( 'L', stderr );
	if ( performance_report )
	    putc( 'p', stderr );
	if ( spprdflt )
	    putc( 's', stderr );
	if ( use_stdout )
	    putc( 't', stderr );
	if ( trace )
	    putc( 'T', stderr );
	if ( printstats )
	    putc( 'v', stderr );        /* always true! */
	if ( csize == 256 )
	    putc( '8', stderr );

	fprintf( stderr, " -C" );

	if ( fulltbl )
	    putc( 'f', stderr );
	if ( fullspd )
	    putc( 'F', stderr );
	if ( useecs )
	    putc( 'e', stderr );
	if ( usemecs )
	    putc( 'm', stderr );

	if ( outputfilename )
	    fprintf( stderr, " -o%s", outputfilename );

	if ( headerfilename )
	    fprintf( stderr, " -h%s", headerfilename );

	if ( includefilename && headerfilename &&
	     ! strcmp(includefilename,headerfilename ))
	    fprintf( stderr, " -g%s", includefilename );

	if ( skelname && strcmp( skelname, DEFAULT_SKELETON_FILE ) )
	    fprintf( stderr, " -S%s", skelname );

	if ( strcmp( skelname, DEFAULT_SKELETONHEADER_FILE ) )
	    fprintf( stderr, " -H%s", skelheaderfilename );

	putc( '\n', stderr );

	fprintf( stderr, "  %d/%d NFA states\n", lastnfa, current_mns );
	fprintf( stderr, "  %d/%d DFA states (%d words)\n", lastdfa,
		 current_max_dfas, totnst );
	fprintf( stderr,
		 "  %d rules\n", num_rules - 1 /* - 1 for def. rule */ );

	if ( num_backtracking == 0 )
	    fprintf( stderr, "  No backtracking\n" );
	else if ( fullspd || fulltbl )
	    fprintf( stderr, "  %d backtracking (non-accepting) states\n",
		     num_backtracking );
	else
	    fprintf( stderr, "  compressed tables always backtrack\n" );

	if ( bol_needed )
	    fprintf( stderr, "  Beginning-of-line patterns used\n" );

	fprintf( stderr, "  %d/%d start conditions\n", lastsc,
		 current_max_scs );
	fprintf( stderr, "  %d epsilon states, %d double epsilon states\n",
		 numeps, eps2 );

	if ( lastccl == 0 )
	    fprintf( stderr, "  no character classes\n" );
	else
	    fprintf( stderr,
	"  %d/%d character classes needed %d/%d words of storage, %d reused\n",
		     lastccl, current_maxccls,
		     cclmap[lastccl] + ccllen[lastccl],
		     current_max_ccl_tbl_size, cclreuse );

	fprintf( stderr, "  %d state/nextstate pairs created\n", numsnpairs );
	fprintf( stderr, "  %d/%d unique/duplicate transitions\n",
		 numuniq, numdup );

	if ( fulltbl )
	    {
	    tblsiz = lastdfa * numecs;
	    fprintf( stderr, "  %d table entries\n", tblsiz );
	    }

	else
	    {
	    tblsiz = 2 * (lastdfa + numtemps) + 2 * tblend;

	    fprintf( stderr, "  %d/%d base-def entries created\n",
		     lastdfa + numtemps, current_max_dfas );
	    fprintf( stderr, "  %d/%d (peak %d) nxt-chk entries created\n",
		     tblend, current_max_xpairs, peakpairs );
	    fprintf( stderr,
		     "  %d/%d (peak %d) template nxt-chk entries created\n",
		     numtemps * nummecs, current_max_template_xpairs,
		     numtemps * numecs );
	    fprintf( stderr, "  %d empty table entries\n", nummt );
	    fprintf( stderr, "  %d protos created\n", numprots );
	    fprintf( stderr, "  %d templates created, %d uses\n",
		     numtemps, tmpuses );
	    }

	if ( useecs )
	    {
	    tblsiz = tblsiz + csize;
	    fprintf( stderr, "  %d/%d equivalence classes created\n",
		     numecs, csize );
	    }

	if ( usemecs )
	    {
	    tblsiz = tblsiz + numecs;
	    fprintf( stderr, "  %d/%d meta-equivalence classes created\n",
		     nummecs, csize );
	    }

	fprintf( stderr, "  %d (%d saved) hash collisions, %d DFAs equal\n",
		 hshcol, hshsave, dfaeql );
	fprintf( stderr, "  %d sets of reallocations needed\n", num_reallocs );
	fprintf( stderr, "  %d total table entries needed\n", tblsiz );
	}

#ifndef VMS
    exit( status );
#else
    exit( status + 1 );
#endif
    }


/* flexinit - initialize flex
 *
 * synopsis
 *    int argc;
 *    char **argv;
 *    flexinit( argc, argv );
 */

void flexinit( argc, argv )
int argc;
char **argv;

    {
    int i, sawcmpflag;
    char *arg, *flex_gettime();
    char *tmp_action = NULL;
    printstats = syntaxerror = trace = spprdflt = interactive = caseins = false;
    backtrack_report = performance_report = ddebug = fulltbl = fullspd = false;
    yymore_used = continued_action = reject = false;
    yymore_really_used = reject_really_used = false;
    gen_line_dirs = usemecs = useecs = true;

    sawcmpflag = false;
    use_stdout = false;

    csize = DEFAULT_CSIZE;
    starttime = flex_gettime();

    program_name = argv[0];

    /* read flags */
    for ( --argc, ++argv; argc ; --argc, ++argv )
	{
	if ( argv[0][0] != '-' || argv[0][1] == '\0' )
	    break;

	arg = argv[0];

	for ( i = 1; arg[i] != '\0'; ++i )
	    switch ( arg[i] )
		{
		case 'a':
		    if ( i != 1 )
			flexerror( "-a flag must be given separately" );
		    tmp_action = &arg[i+1];
		    goto get_next_arg;
		case 'b':
		    backtrack_report = true;
		    break;

		case 'c':
		    fprintf( stderr,
	"%s: Assuming use of deprecated -c flag is really intended to be -C\n",
			     program_name );

		    /* fall through */

		case 'C':
		    if ( i != 1 )
			flexerror( "-C flag must be given separately" );

		    if ( ! sawcmpflag )
			{
			useecs = false;
			usemecs = false;
			fulltbl = false;
			sawcmpflag = true;
			}

		    for ( ++i; arg[i] != '\0'; ++i )
			switch ( arg[i] )
			    {
			    case 'e':
				useecs = true;
				break;

			    case 'F':
				fullspd = true;
				break;

			    case 'f':
				fulltbl = true;
				break;

			    case 'm':
				usemecs = true;
				break;

			    default:
				fprintf(stderr, "unknown -C option '%c'",
					(int) arg[i] );
				flexinfo(1);
				break;
			    }

		    goto get_next_arg;

		case 'd':
		    ddebug = true;
		    break;

		case 'f':
		    useecs = usemecs = false;
		    fulltbl = true;
		    break;

		case 'F':
		    useecs = usemecs = false;
		    fullspd = true;
		    break;
		case 'g':
		    if ( i != 1 )
			flexerror( "-g flag must be given separately" );

		    includefilename = arg + i + 1;
		    goto get_next_arg;
		case 'h':
		    if ( i != 1 )
			flexerror( "-h flag must be given separately" );

		    headerfilename = arg + i + 1;
		    goto get_next_arg;
		case 'o':
		    if ( i != 1 )
			flexerror( "-o flag must be given separately" );

		    outputfilename = arg + i + 1;
		    goto get_next_arg;

		case 'H':
		    if ( i != 1 )
			flexerror( "-H flag must be given separately" );

		    skelheaderfilename = arg + i + 1;
		    goto get_next_arg;


		case 'I':
		    interactive = true;
		    break;

		case 'i':
		    caseins = true;
		    break;

		case 'L':
		    gen_line_dirs = false;
		    break;

		case 'n':
		    /* stupid do-nothing deprecated option */
		    break;

		case 'p':
		    performance_report = true;
		    break;

		case 'S':
		    if ( i != 1 )
			flexerror( "-S flag must be given separately" );

		    skelname = arg + i + 1;
		    goto get_next_arg;

		case 's':
		    spprdflt = true;
		    break;

		case 't':
		    use_stdout = true;
		    break;

		case 'T':
		    trace = true;
		    break;

		case 'v':
		    printstats = true;
		    break;

		case '8':
		    csize = CSIZE;
		    break;

		case '?':
		    flexinfo(0);
		    break;

		default:
		    fprintf(stderr, "unknown flag '%c'", (int) arg[i] );
		    flexinfo(1);
		    break;
		}

get_next_arg: /* used by -C and -S flags in lieu of a "continue 2" control */
	;
	}

    if ( (fulltbl || fullspd) && usemecs )
	flexerror( "full table and -Cm don't make sense together" );

    if ( (fulltbl || fullspd) && interactive )
	flexerror( "full table and -I are (currently) incompatible" );

    if ( fulltbl && fullspd )
	flexerror( "full table and -F are mutually exclusive" );
    if( use_stdout && outputfilename )
	flexerror( "-t (generate to stdout) and -o (generate to file) are mutually exclusive" );
    if ( ! skelname )
	{
	skelname=DEFAULT_SKELETON_FILE;
	}

    if ( ! skelheaderfilename )
	{
	skelheaderfilename=DEFAULT_SKELETONHEADER_FILE;
	}
    if ( ! use_stdout )
	{FILE *prev_stdout;
	if(outputfilename && outputfilename[0] ) outfile=outputfilename;
	prev_stdout = freopen( outfile, "w", stdout );

	if ( prev_stdout == NULL )
	    lerrsf( "could not create %s", outfile );

	outfile_created = 1;
	}

    num_input_files = argc;
    input_files = argv;
    set_input_file( num_input_files > 0 ? input_files[0] : NULL );
    if ( (skelheaderfile = fopen( skelheaderfilename, "r" )) == NULL )
#ifdef MS_DOS
     if(getenv("INIT")!=NULL)
     {char *t;
      t=malloc(strlen(getenv("INIT"))+strlen(skelheaderfilename)+2);
      strcpy(t,getenv("INIT"));
      strcat(t,"/");
      strcat(t,skelheaderfilename);
      skelheaderfilename=t;
      if((skelheaderfile = fopen( skelheaderfilename, "r" )) == NULL)
	lerrsf( "can't open skeleton header file %s", skelheaderfilename );
     }
     else
#endif
	lerrsf( "can't open skeleton header file %s", skelheaderfilename );
    if(headerfilename==NULL) headerfile=stdout;
    else
     {
      if(headerfilename[0]=='\0')
       {char **suffix;
	headerfilename=malloc(strlen(outfile)+3);
	strcpy(headerfilename,outfile);
	for(suffix=c_suffixes;*suffix;suffix++)
	/* try to detect .c .cpp options */
	  {if(strlen(headerfilename)>strlen(*suffix)
	      && strcmp(headerfilename+
		   strlen(headerfilename)-strlen(*suffix),*suffix)==0
	      )
	    { *(headerfilename+strlen(headerfilename)-strlen(*suffix))='\0';
	     break;}
	  };
	strcat(headerfilename,".h");
	}
      if(includefilename==NULL)
	 includefilename=headerfilename;
      if ( (headerfile = fopen( headerfilename, "w" )) == NULL )
	lerrsf( "can't open header file %s", headerfilename );
      };
    if ( backtrack_report )
	{
#ifndef SHORT_FILE_NAMES
	backtrack_file = fopen( "lex.backtrack", "w" );
#else
	backtrack_file = fopen( "lex.bck", "w" );
#endif
	if ( backtrack_file == NULL )
	    flexerror( "could not create lex.backtrack" );
	}

    else
	backtrack_file = NULL;


    lastccl = 0;
    lastsc = 0;

    /* initialize the statistics */

    if ( (skelfile = fopen( skelname, "r" )) == NULL )
#ifdef MS_DOS
       if(getenv("INIT")!=NULL)
     {char *t;
      t=malloc(strlen(getenv("INIT"))+strlen(skelname)+2);
      strcpy(t,getenv("INIT"));
      strcat(t,"/");
      strcat(t,skelname);
      skelname=t;
      if((skelfile = fopen(skelname , "r" )) == NULL)
	lerrsf( "can't open skeleton file %s", skelname );
     }
     else
#endif
	lerrsf( "can't open skeleton file %s", skelname );
    if(tmp_action)
     {char *ftmp;
      ftmp=malloc(strlen(tmp_action)+1+32);
      strcpy(ftmp,tmp_action);
#ifndef SHORT_FILE_NAMES
	(void) strcat( ftmp, "/flexXXXXXX" );
#else
	(void) strcat( ftmp, "/flXXXXXX.tmp" );
#endif
      action_file_name=ftmp;
     }
    if ( action_file_name == NULL )
	{
	static char temp_action_file_name[32];

#ifndef SHORT_FILE_NAMES
	(void) strcpy( temp_action_file_name, "/tmp/flexXXXXXX" );
#else
	(void) strcpy( temp_action_file_name, "flXXXXXX.tmp" );
#endif
	action_file_name = temp_action_file_name;
	}

#ifndef SHORT_FILE_NAMES
    if ( (temp_action_file = fdopen( mkstemp( action_file_name ), "w+" )) == NULL )
#else
    if ( (temp_action_file = fdopen( mkstemps( action_file_name, 4 ), "w+" )) == NULL )
#endif
	flexerror( "can't open temporary action file" );

    lastdfa = lastnfa = num_rules = numas = numsnpairs = tmpuses = 0;
    numecs = numeps = eps2 = num_reallocs = hshcol = dfaeql = totnst = 0;
    numuniq = numdup = hshsave = eofseen = datapos = dataline = 0;
    num_backtracking = onesp = numprots = 0;
    variable_trailing_context_rules = bol_needed = false;

    linenum = sectnum = 1;
    firstprot = NIL;

    /* used in mkprot() so that the first proto goes in slot 1
     * of the proto queue
     */
    lastprot = 1;

    if ( useecs )
	{ /* set up doubly-linked equivalence classes */
	/* We loop all the way up to csize, since ecgroup[csize] is the
	 * position used for NUL characters
	 */
	ecgroup[1] = NIL;

	for ( i = 2; i <= csize; ++i )
	    {
	    ecgroup[i] = i - 1;
	    nextecm[i - 1] = i;
	    }

	nextecm[csize] = NIL;
	}

    else
	{ /* put everything in its own equivalence class */
	for ( i = 1; i <= csize; ++i )
	    {
	    ecgroup[i] = i;
	    nextecm[i] = BAD_SUBSCRIPT; /* to catch errors */
	    }
	}

    set_up_initial_allocations();
    }

 int flexinfo(status)
 int status;
 {
    fprintf(stderr,"Syntax  : flex [options...] inp_file\n");
    fprintf(stderr,"Function: fast lexical analyzer generator\n");
    fprintf(stderr,"Version : %s\n",flex_version);
    fprintf(stderr,"Options :\n");
    fprintf(stderr,"	-a dir_path  : directory path for temporary files\n");
    fprintf(stderr,"	-b  : generate backtracking information to lex.backtrack\n");
    fprintf(stderr,"	-c  : compressed table, no equiv., no meta equiv.classes\n");
    fprintf(stderr,"	-C  : compressed table, no equiv., no meta equiv.classes\n");
    fprintf(stderr,"	-Ce : compressed table, with equivalence classes\n");
    fprintf(stderr,"	-Cem: compressed table, with equivalence and meta equiv. classes\n");
    fprintf(stderr,"	-Cm: compressed table, with meta equivalence classes\n");
    fprintf(stderr,"	-Cm: compressed table, with meta equivalence classes\n");
    fprintf(stderr,"	-CF :  alternate fast table\n");
    fprintf(stderr,"	-Cf :  full table\n");
    fprintf(stderr,"	-CFe : alternate fast table, with equivalence classes\n");
    fprintf(stderr,"	-Cfe : full table, with equivalence classes\n");
    fprintf(stderr,"	-F  : fast table\n");
    fprintf(stderr,"	-f  : full (not compressed) table\n");
    fprintf(stderr,"	-d  : generate debugging scanner\n");
    fprintf(stderr,"	-I  : generate interactive scanner\n");
    fprintf(stderr,"	-i  : generate case-insensitive scanner\n");
    fprintf(stderr,"	-L  : supress #line directives\n");
 /*   fprintf(stderr,"	-n hexnum : generate scanner using <hexnum> as newline char.\n");*/
    fprintf(stderr,"	-p  : generate performance report to stderr\n");
    fprintf(stderr,"	-S skeleton_path : file path for skeleton file\n");
    fprintf(stderr,"	-H header_skeleton_path : file path for header skeleton file\n");
    fprintf(stderr,"	-s  : suppress echo of unmatched scanner input to stdout\n");
    fprintf(stderr,"	-T  : run flex in trace mode\n");
    fprintf(stderr,"	-t  : place result on stdout \n");
    fprintf(stderr,"	-v  : print statistics of generated scanner\n");
    fprintf(stderr,"	-o code_file  : set name of output code file\n");
    fprintf(stderr,"	-h [header_file]  : generate a separate header_file\n");
    fprintf(stderr,"	-g include_file  : name of header to include (default= same as -h)\n");
    fprintf(stderr,"	-?   : this help\n");
    fprintf(stderr,"	default =  -Cem\n");
    exit(status);
    return 0;
 }



/* readin - read in the rules section of the input file(s)
 *
 * synopsis
 *    readin();
 */

void readin()

    {
    char *ch_type;

    if(headerfilename!=NULL)
     {char symb[32],ch;
      char *p=headerfilename;
      int i=0;symb[0]='\0';
      while(*p)
       {
	if(*p=='/'
#ifdef MS_DOS
	   || *p=='\\' || *p==':'
#endif
	   ) {symb[0]='\0';i=0;}
	else {
	 if((*p>='A' && *p<='Z')||((*p>='a' && *p<='z'))
             ||((*p>='0' && *p<='9'))||(ch=='_'))
          ch=*p;
	 else
          ch='_';
	 if(i<sizeof(symb)-1)
	  {symb[i++]=ch;symb[i]='\0';}
	 }
	p++;
       };
      fprintf(headerfile,"#ifndef FLEX_HEADER_%s\n",symb );
      fprintf(headerfile,"#define FLEX_HEADER_%s\n",symb );
     }
     if ( csize == 256 )
      ch_type="unsigned char";
     else
      ch_type="char";
    if ( ddebug )
      fprintf(stdout, "#define FLEX_DEBUG\n" );
    fprintf(stdout, "#define YY_CHAR %s\n",ch_type );
    skelout();

    if ( yyparse() )
	{
	pinpoint_message( "fatal parse error" );
	flexend( 1 );
	}

    if ( xlation )
	{
	numecs = ecs_from_xlation( ecgroup );
	useecs = true;
	}

    else if ( useecs )
	numecs = cre8ecs( nextecm, ecgroup, csize );

    else
	numecs = csize;

    /* now map the equivalence class for NUL to its expected place */
    ecgroup[0] = ecgroup[csize];
    NUL_ec = abs( ecgroup[0] );

    if ( useecs )
	ccl2ecl();
    line_directive_out( headerfile );
    if(headerfilename!=NULL) fprintf(headerfile,"#endif\n" );
    header_skeleton_out();

    }



/* set_up_initial_allocations - allocate memory for internal tables */

void set_up_initial_allocations()

    {
    current_mns = INITIAL_MNS;
    firstst = allocate_integer_array( current_mns );
    lastst = allocate_integer_array( current_mns );
    finalst = allocate_integer_array( current_mns );
    transchar = allocate_integer_array( current_mns );
    trans1 = allocate_integer_array( current_mns );
    trans2 = allocate_integer_array( current_mns );
    accptnum = allocate_integer_array( current_mns );
    assoc_rule = allocate_integer_array( current_mns );
    state_type = allocate_integer_array( current_mns );

    current_max_rules = INITIAL_MAX_RULES;
    rule_type = allocate_integer_array( current_max_rules );
    rule_linenum = allocate_integer_array( current_max_rules );

    current_max_scs = INITIAL_MAX_SCS;
    scset = allocate_integer_array( current_max_scs );
    scbol = allocate_integer_array( current_max_scs );
    scxclu = allocate_integer_array( current_max_scs );
    sceof = allocate_integer_array( current_max_scs );
    scname = allocate_char_ptr_array( current_max_scs );
    actvsc = allocate_integer_array( current_max_scs );

    current_maxccls = INITIAL_MAX_CCLS;
    cclmap = allocate_integer_array( current_maxccls );
    ccllen = allocate_integer_array( current_maxccls );
    cclng = allocate_integer_array( current_maxccls );

    current_max_ccl_tbl_size = INITIAL_MAX_CCL_TBL_SIZE;
    ccltbl = allocate_character_array( current_max_ccl_tbl_size );

    current_max_dfa_size = INITIAL_MAX_DFA_SIZE;

    current_max_xpairs = INITIAL_MAX_XPAIRS;
    nxt = allocate_integer_array( current_max_xpairs );
    chk = allocate_integer_array( current_max_xpairs );

    current_max_template_xpairs = INITIAL_MAX_TEMPLATE_XPAIRS;
    tnxt = allocate_integer_array( current_max_template_xpairs );

    current_max_dfas = INITIAL_MAX_DFAS;
    base = allocate_integer_array( current_max_dfas );
    def = allocate_integer_array( current_max_dfas );
    dfasiz = allocate_integer_array( current_max_dfas );
    accsiz = allocate_integer_array( current_max_dfas );
    dhash = allocate_integer_array( current_max_dfas );
    dss = allocate_int_ptr_array( current_max_dfas );
    dfaacc = allocate_dfaacc_union( current_max_dfas );

    nultrans = (int *) 0;
    }
