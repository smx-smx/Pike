#!/usr/local/bin/pike

/* $Id: test_pike.pike,v 1.27 1999/08/30 06:21:17 hubbe Exp $ */

import Stdio;

#if !efun(_verify_internals)
#define _verify_internals()
#endif

int foo(string opt)
{
  if(opt=="" || !opt) return 1;
  return (int)opt;
}

mapping(string:int) cond_cache=([]);

#if constant(thread_create)
#define HAVE_DEBUG
#endif

void bzot(string test)
{
  int line=1;
  int tmp=strlen(test)-1;
  while(test[tmp]=='\n') tmp--;
  foreach(test[..tmp]/"\n",string s)
    werror("%3d: %s\n",line++,s);
}

array find_testsuites(string dir)
{
  array(string) ret=({});
  if(array(string) s=get_dir(dir||"."))
  {
    foreach(s, string file)
      {
	string name=combine_path(dir||"",file);
	if(file_size(name)==-2)
	  ret+=find_testsuites(name);
      }
    
    foreach(s, string file)
      {
	switch(file)
	{
	  case "testsuite":
	  case "module_testsuite":
	    ret+=({ combine_path(dir||"",file) });
	}
      }
  }
  return ret;
}

int main(int argc, string *argv)
{
  int e, verbose, successes, errors, t, check;
  int skipped;
  string *tests,tmp;
  program testprogram;
  int start, fail, mem;
  int loop=1;
  int end=0x7fffffff;
  string extra_info="";
  int shift;

#if constant(signal) && constant(signum)
  if(signum("SIGQUIT")>=0)
  {
    signal(signum("SIGQUIT"),lambda()
	   {
	     master()->handle_error( ({"\nSIGQUIT recived, printing backtrace and continuing.\n",backtrace() }) );
	   });
  }
#endif

  string *args=backtrace()[0][3];
  array(string) testsuites=({});
  args=args[..sizeof(args)-1-argc];
  add_constant("RUNPIKE",Array.map(args,Process.sh_quote)*" ");

  foreach(Getopt.find_all_options(argv,aggregate(
    ({"help",Getopt.NO_ARG,({"-h","--help"})}),
    ({"verbose",Getopt.NO_ARG,({"-v","--verbose"})}),
    ({"start",Getopt.HAS_ARG,({"-s","--start-test"})}),
    ({"end",Getopt.HAS_ARG,({"--end-after"})}),
    ({"fail",Getopt.MAY_HAVE_ARG,({"-f","--fail"})}),
    ({"loop",Getopt.MAY_HAVE_ARG,({"-l","--loop"})}),
    ({"trace",Getopt.MAY_HAVE_ARG,({"-t","--trace"})}),
    ({"check",Getopt.MAY_HAVE_ARG,({"-c","--check"})}),
    ({"mem",Getopt.MAY_HAVE_ARG,({"-m","--mem","--memory"})}),
    ({"auto",Getopt.MAY_HAVE_ARG,({"-a","--auto"})}),
#ifdef HAVE_DEBUG
    ({"debug",Getopt.MAY_HAVE_ARG,({"-d","--debug"})}),
#endif
    )),array opt)
    {
      switch(opt[0])
      {
	case "help":
	  werror("Usage: "+argv[e]+" [-v | --verbose] [-h | --help] [-t <testno>] <testfile>\n");
	  return 0;

	case "verbose": verbose+=foo(opt[1]); break;
	case "start": start=foo(opt[1]); start--; break;
	case "end": end=foo(opt[1]); break;
	case "fail": fail+=foo(opt[1]); break;
	case "loop": loop+=foo(opt[1]); break;
	case "trace": t+=foo(opt[1]); break;
	case "check": check+=foo(opt[1]); break;
	case "mem": mem+=foo(opt[1]); break;

	case "auto":
	  testsuites=find_testsuites(".");
	  break;

#ifdef HAVE_DEBUG
	case "debug":
	{
	  object p=Stdio.Port();
	  p->bind(0);
	  werror("Debug port is: %s\n",p->query_address());
	  sscanf(p->query_address(),"%*s %d",int portno);
	  extra_info+=sprintf(" dport:%d",portno);
	  thread_create(lambda(object p){
	    while(p)
	    {
	      if(object o=p->accept())
	      {
		object q=Stdio.FILE();
		q->assign(o);
		destruct(o);
		Tools.Hilfe.GenericHilfe(q,q);
	      }
	    }
	  },p);
	}
#endif
      }
    }

  argv=Getopt.get_args(argv,1)+testsuites;
  if(sizeof(argv)<1)
  {
    if(!tmp)
    {
      werror("No tests?\n");
      exit(1);
    }
  }

  while(loop--)
  {
    successes=errors=0;
    for(int f=1;f<sizeof(argv);f++)
    {
      tmp=read_bytes(argv[f]);
      if(!tmp)
      {
	werror("Failed to read test file, errno="+errno()+".\n");
	exit(1);
      }
      
      tests=tmp/"\n....\n";
      tmp=0;
      tests=tests[0..sizeof(tests)-2];
      
      werror("Doing tests in %s (%d tests)\n",argv[f],sizeof(tests));
      
	for(e=start;e<sizeof(tests);e++)
	{
	  werror("%6d\r",e+1);

	  string test,condition;
	  int type;
	  object o;
	  mixed a,b;
	
	  if(check) _verify_internals();
	
	  test=tests[e];	
	  if(sscanf(test,"COND %s\n%s",condition,test)==2)
	  {
	    int tmp;
	    if(!(tmp=cond_cache[condition]))
	    {
	      tmp=!!(clone(compile_string("mixed c() { return "+condition+"; }","Cond "+(e+1)))->c());
	      if(!tmp) tmp=-1;
	      cond_cache[condition]=tmp;
	    }
	  
	    if(tmp==-1)
	    {
	      if(verbose)
		werror("Not doing test "+(e+1)+"\n");
	      successes++;
	      skipped++;
	      continue;
	    }
	  }
	
	  sscanf(test,"%s\n%s",type,test);
	  sscanf(type,"%*s expected result: %s",type);
	
	  if(verbose)
	  {
	    werror("Doing test %d (%d total)%s\n",e+1,successes+errors+1,extra_info);
	    if(verbose>1) bzot(test);
	  }

	  if(check > 1) _verify_internals();
	
	  shift++;
	  shift%=3;
	  string fname = argv[f] + ": Test " + (e + 1) +
	    " (shift " + shift + ")";

	  string widener = ([ 0:"",
			    1:"\nint \x30c6\x30b9\x30c8=0;\n",
			    2:"\nint \x10001=0;\n" ])[shift%3];

	  switch(type)
	  {
	    case "COMPILE":
	      if(catch(compile_string(test + widener, fname)))
	      {
		werror(fname + " failed.\n");
		bzot(test);
		errors++;
	      }else{
		successes++;
	      }
	      break;
	    
	    case "COMPILE_ERROR":
	      master()->set_inhibit_compile_errors(1);
	      if(catch(compile_string(test + widener, fname)))
	      {
		successes++;
	      }else{
		werror(fname + " failed.\n");
		bzot(test);
		errors++;
	      }
	      master()->set_inhibit_compile_errors(0);
	      break;
	    
	    case "EVAL_ERROR":
	      master()->set_inhibit_compile_errors(1);
	      if(catch(clone(compile_string(test + widener, fname))->a()))
	      {
		successes++;
	      }else{
		werror(fname + " failed.\n");
		bzot(test);
		errors++;
	      }
	      master()->set_inhibit_compile_errors(0);
	      break;
	    
	    default:
	      mixed err;
	      if (err = catch{
		o=clone(compile_string(test + widener,fname));
	    
		if(check > 1) _verify_internals();
	    
		a=b=0;
		if(t) trace(t);
		if(functionp(o->a)) a=o->a();
		if(functionp(o->b)) b=o->b();
		if(t) trace(0);
		if(check > 1) _verify_internals();
	      }) {
		werror(fname + " failed.\n");
		bzot(test);
		if (arrayp(err) && sizeof(err) && stringp(err[0])) {
		  werror("Error: " + master()->describe_backtrace(err));
		}
		if (objectp(err)) {
		  werror("Error: " + master()->describe_backtrace(err));
		}
		errors++;
		break;
	      }
	    
	      switch(type)
	      {
		case "FALSE":
		  if(a)
		  {
		    werror(fname + " failed.\n");
		    bzot(test);
		    werror(sprintf("o->a(): %O\n",a));
		    errors++;
		  }else{
		    successes++;
		  }
		  break;
		
		case "TRUE":
		  if(!a)
		  {
		    werror(fname + " failed.\n");
		    bzot(test);
		    werror(sprintf("o->a(): %O\n",a));
		    errors++;
		  }else{
		    successes++;
		  }
		  break;
		
		case "RUN":
		  successes++;
		  break;
		
		case "EQ":
		  if(a!=b)
		  {
		    werror(fname + " failed.\n");
		    bzot(test);
		    werror(sprintf("o->a(): %O\n",a));
		    werror(sprintf("o->b(): %O\n",b));
		    errors++;
		  }else{
		    successes++;
		  }
		  break;
		
		case "EQUAL":
		  if(!equal(a,b))
		  {
		    werror(fname + " failed.\n");
		    bzot(test);
		    werror(sprintf("o->a(): %O\n",a));
		    werror(sprintf("o->b(): %O\n",b));
		    errors++;
		  }else{
		    successes++;
		  }
		  break;
		
		default:
		  werror(sprintf("%s: Unknown test type (%O).\n", fname, type));
		  errors++;
	      }
	  }
	
	  if(check > 2) _verify_internals();
	
	  if(fail && errors)
	    exit(1);

	  if(!--end) exit(0);
	
	  a=b=0;
      }
	werror("             \r");
    }
    if(mem)
    {
      int total;
      tests=0;
      gc();
      mapping tmp=_memory_usage();
      write(sprintf("%-10s: %6s %10s\n","Category","num","bytes"));
      foreach(sort(indices(tmp)),string foo)
	{
	  if(sscanf(foo,"%s_bytes",foo))
	  {
	    write(sprintf("%-10s: %6d %10d\n",
			  foo+"s",
			  tmp["num_"+foo+"s"],
			  tmp[foo+"_bytes"]));
	    total+=tmp[foo+"_bytes"];
	  }
	}
      write(sprintf("%-10s: %6s %10d\n",
		    "Total",
		    "",
		    total));
    }
  }
  if(errors || verbose)
  {
    werror("Failed tests: "+errors+".\n");
  }
      
  werror("Total tests: %d  (%d tests skipped)\n",successes+errors,skipped);

  return errors;
}
