#!/bin/sh
#
# shell pattern matching benchmark, by avih. License: MIT.


echo()  { printf %s\\n "$*"; }
echon() { printf %s    "$*"; }

SELF=${0##*[\\/]}

HELP="\
Usage: $SELF [OPTIONS]
Measure duration of shell pattern matches with 5 text lines.
Lines are ~ 80 chars each. There are ~ 50 builtin patterns.

Matched-lines are printed as e.g. '(M:1-34-)' (lines 1,3,4).
The builtin patterns know which of the default lines should match,
and print 'OK', or e.g. 'E:--3-5' (expected: 3,5), or 'unknown'
for other combos. Exit code is 0 if there are no 'E:.....' prints.

The builtin patterns have increasing number of '*'. This will
blow with exponential implementations. Just abort it manuallly.

  -s      Use short input lines (3-10 chars each) instead of ~ 80 chars.
  -i      Use the first 5 lines from stdin instead of builtin lines.
  -p PAT  Use [newline-separated] PAT instead of the builtin patterns.
  -n N    Iterate the 5-lines N x 1000 times. Default: 10 (50K matches).
  -d      Don't benchmark. Report matched lines, and errors if possible.
  -o      Print matched lines after each pettern result.
  -m MODE Measurement mode:
          'c': Use print-ms cmd. Tries \$NOWCMD, date +%s%3N, few more.
          't': Use shell (user) 'times' - typically 10 ms resolution.
          'n': Don't measure internally and reduce overhead (print only
               progress). Overall duration can be measured externally.
          Default: try 'c', and if no usable command is found, use 't'.
"


# input lines

DEFLINES="\
Lorem	ipsum dolor sit amet, consectetur adipiscing elit, [] do eiusmod tempor
incididunt ut labore_et dolore magna aliqua. Ut enim ad minim veniam, xyzquis
nostrud exercitation ullamco laboris nisi [!] aliquip ex commodoxyz] consequat.
Duis aute irure dolor - reprehenderit in voluptate velit esse cillum dolore
eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt
"

SHORTLINES="\
Lorem
ut.
cupidatat.
aute
esse 
"

deflines()   { printf %s "$DEFLINES"; }
shortlines() { printf %s "$SHORTLINES"; }
stdinlines() { head -5; }


# builtin patterns

# a/b, [c/]d/e, and f/g are equivalent, made same-length to reduce length bias

a='[0-9A-Za-z]'
b='[[:alnum:]]'  # 1st in char-classes (lexicographic) list - best-case

c='[a-z]'        # same as next patterns, but shorter to parse
d='[a-bc-de-z]'
e='[[:lower:]]'  # 6th in char-classes list

f='[0-9A-Fa-ef]'
g='[[:xdigit:]]'  # 12th in char-classes list - worst-case

# the test patterns starts at the 7th char of the line. char 6 is space.
# chars 1-5 are either space if this pattern is not tested for correctness,
# else the expected matched line numbers with the long builtin lines,
# e.g. all lines: 12345, lines 2 and 5: -2--5, no line: -----, etc.
PATTERNS="\
----- 
12345 *
----- @
1---- Lorem*
-2--- *quis
--3-- *nisi*
--3-- *[fobar]xyz*
--3-- *[foobarfoobarfoobarfoobar]xyz*
-2--- *[!foobarfoobarfoobarfoobar]xyz*
12345 *[hello world, this is a test]*
12345 *[!hello world, this is a test]*
1-3-- *[*
1---- *[]*
--3-- *[!]*
-23-5 *[!]].*
1---- *[![:print:]]*
-23-- *z*
---4- *-*
---4- *[-]*
-234- *[-z]*
-234- *[z-]*
-234- *[-z-]*
1-3-- *]*
1-3-- *[]]*
1-34- *[]-]*
123-- *[]-_]*
1234- *[]-_-]*
12-45 *$a$a
12-45 *$b$b
12-45 *$c$c
12-45 *$d$d
12-45 *$e$e
----- *$f$f
----- *$g$g
12-45 *$a*$a
12-45 *$b*$b
12-45 *$c*$c
12-45 *$d*$d
12-45 *$e*$e
---4- *$f*$f
---4- *$g*$g
12-45 *$a*$a*$a
12-45 *$b*$b*$b
12-45 *$c*$c*$c
12-45 *$d*$d*$d
12-45 *$e*$e*$e
---4- *$f*$f*$f
---4- *$g*$g*$g
12-45 *$a*$a*$a*$a
12-45 *$b*$b*$b*$b
12-45 *$c*$c*$c*$c
12-45 *$d*$d*$d*$d
12-45 *$e*$e*$e*$e
---4- *$f*$f*$f*$f
---4- *$g*$g*$g*$g
"


# measurement. we have xtime_mscmd and xtime_times. both run "$@" and
# set $dt to the duration, as "N ms" or shell-user-time str from "times".
# unfortunately none is ideal. the former is non-posix, and the latter
# is typically 10ms resolution. 'time sh -c ...' is also usually 10ms res,
# and would have required even more setup work (generate bench script per
# pattern, figure out what is the current shell so that we can invoke it).

# no standard way to print timestamp in ms, so try few options.
# xtime below tries to compensate latency. also serves as warmup for xtime.
init_measure() {
    while IFS= read -r cmd_ms; do
        { x=$(eval "$cmd_ms"); } 2>/dev/null
        case $x in [1-9]*[0-9])
            [ "${#x}" -gt 11 ] && break  # 10 digits is seconds-timestamp
        esac
        cmd_ms=
    done <<- CMDS
	${NOWCMD-}
	date  +%s%3N
	gdate +%s%3N
	perl -MPOSIX=strftime -MTime::HiRes=time -le "print int(time*1000)"
	python  -c "from time import time; print (int(time()*1000))"
	python3 -c "from time import time; print (int(time()*1000))"
	nodejs -e "console.log(Date.now())"
	node   -e "console.log(Date.now())"
	CMDS

    if ! [ "$cmd_ms" ]; then
        >&2 echo "$SELF: error - cannot find timestamp-ms command. (try -h)"
        >&2 echo "(tried gnu date, and perl/python/nodejs)"
        exit 1
    fi
}
now_ms() {
    eval "$cmd_ms"
}
xtime_mscmd() {
    t0=$(now_ms)
    "$@"
    t1=$(now_ms)
    t2=$(now_ms)
    dt=$((t1 - t0 - (t2 - t1)))  # latency-compensated-ish
    [ $dt -ge 0 ] || dt=0
    dt="$dt ms"
}


# theoretically ideal, but sometimes only 10ms res, and sometimes doesn't
# reset despite executing in a subshell (ksh93). also, runs "$@" in subshell.
xtime_times() {
    if [ "${KSH_VERSION+x}" ]; then
        # ksh93 has no-fork optimization which breaks times. force fork.
        # ksh clones have this var too - openbsd/mksh, but it still works.
        dt=$(ulimit -c 0; "$@"; times)
    else
        dt=$("$@"; times)
    fi
    dt=${dt%% *}  # 1st element - "shell user time"
}


# print matched lines for pattern $1, and benchmark pattern $1

print_matches() {
    while IFS= read -r line; do
        case $line in $1) echo "$line"; esac
    done
}

# positional parameters are usually quicker to access than named vars.
# the action on match is intentionally empty to remove matches duration
# bias due to additional command. no known shell optimizes out such lines.
benchmark() {
    set -- "$1" "$L1" "$L2" "$L3" "$L4" "$L5"
    n=$((1+N*1000))
    until [ $((n=n-1)) = 0 ]; do
        case $2 in $1) ;; esac
        case $3 in $1) ;; esac
        case $4 in $1) ;; esac
        case $5 in $1) ;; esac
        case $6 in $1) ;; esac
    done
}


# main

N=10 D= O= measure= input_lines=deflines cpat=
opts=hidm:n:sp:o
while getopts $opts o; do
    case $o in
    s)  input_lines=shortlines ;;
    i)  input_lines=stdinlines ;;
    n)  N=$OPTARG ;;
    o)  O=yes ;;
    d)  D=yes ;;
    p)  PATTERNS=$OPTARG cpat=yes ;;  # custom patterns
    m)  case $OPTARG in
        c) measure=xtime_mscmd ;;
        t) measure=xtime_times ;;
        n) measure=xtime_none  ;;
        *) >&2 echo "$SELF: invalid -m MODE value    (try -h)"
           exit 1
        esac
        ;;
    h)  echon "$HELP"
        exit
        ;;
    *)  >&2 echo "Usage: $SELF [-$opts]    (try -h)"
        exit 1
    esac
done

if [ "$measure" ]; then
    [ "$measure" = xtime_mscmd ] && init_measure
elif (init_measure 2>/dev/null); then
    measure=xtime_mscmd
    init_measure
else
    measure=xtime_times
fi


# read the input lines to vars L1-L5 to benchmark them without IO
{
    IFS= read -r L1
    IFS= read -r L2
    IFS= read -r L3
    IFS= read -r L4
    IFS= read -r L5
} << EOF
$($input_lines)
EOF

PIN() { printf %s\\n "$L1" "$L2" "$L3" "$L4" "$L5"; }

# prints matched lines as e.g. "1--45"
pmatches() {
    o=
    case $L1 in $1) o=${o}1;; *) o=$o-; esac
    case $L2 in $1) o=${o}2;; *) o=$o-; esac
    case $L3 in $1) o=${o}3;; *) o=$o-; esac
    case $L4 in $1) o=${o}4;; *) o=$o-; esac
    case $L5 in $1) o=${o}5;; *) o=$o-; esac
    echo "$o"
}

# let's go

npat=0 err=
printf %s "$PATTERNS" |
while IFS= read -r x || [ "$x" ]; do
    npat=$((npat+1))
    [ "$cpat" ] && p=$x || p=${x#??????}

    if [ "$measure" = xtime_none ]; then
        # remove overheads. the user will measure overall time on their own.
        benchmark "$p"
        echo $npat
        continue
    fi

    res=$(pmatches "$p")
    if [ "$input_lines" = deflines ] && ! [ "$cpat" ]; then
        expect=${x%" $p"}
        case x$expect in
            x"     ") r="unknown" ;;
             x"$res") r="OK     " ;;
                  x*) r=E:$expect err=yes ;;
        esac
    else
        r=unknown
    fi


    if [ "$D" ]; then
        printf "[%2s] %s (M:%s)  %s\n" $npat "$r" "$res" "'$p'"
    else
        $measure benchmark "$p"
        printf "[%2s] %9s  %s (M:%s)  %s\n" $npat "$dt" "$r" "$res" "'$p'"
    fi


    if [ "$O" ]; then
        PIN | print_matches "$p"
        echo
    fi

    [ -z "$err" ]
done
