#!/usr/bin/ruby

# A very simple Gwyddion plug-in example in Ruby.

# Written by Nenad Ocelic <ocelic _at_ biochem.mpg.de>.
# Public domain.

begin
	require "narray"
rescue LoadError
	exit 1
end
$:.push(ENV['GWYPLUGINLIB'] + '/ruby')
require "gwyddion/dump"
include Gwyddion

# Plug-in information.
RUN_MODES= 'noninteractive', 'with_defaults'
PLUGIN_INFO= [ "invert_narray", "/_Test/Value Invert (Ruby+NArray)"]

def register(args)
	puts PLUGIN_INFO, RUN_MODES
end 

def run( args)
	run_mode= args.shift
	RUN_MODES.member? run_mode or raise "Invalid run mode"
	
	dump= DumpNArray.new args.shift
	a= dump[ '/0/data']
	mirror= a.min + a.max
	dump[ '/0/data']= mirror - a 
	dump.write # filename optional unless changed
end 

fn= ARGV.shift
send fn.intern, ARGV
