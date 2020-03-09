#!/bin/bash -e
#  Daniil Gentili's submission to the VoIP contest.
#  Copyright (C) 2019 Daniil Gentili <daniil@daniil.it>
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <https://www.gnu.org/licenses/>.
#

[ $# -eq 1 ] && {
    lang=$1
} || {
    lang=en-us
}

cd "$(dirname $0)"
cd model

for ogg in ../samples/sample*ogg; do
    wav=$(basename "$ogg" .ogg).wav
    [ ! -f $wav ] && opusdec --rate 16000 $ogg $wav
done

rm -f *txt
cp ../transcript/*txt .
for wav in *wav; do
    txt=$(basename "$wav" .wav).txt
    [ ! -f $txt ] && {
        { sleep 1; play "$wav"; } &
        read -rp "Please tell me what is this person saying: " transcript
        echo "$transcript" > $txt
    }
done

# Convert transcripts and also generate model files
../fixTranscripts -m
cp *txt ../transcript
rm ../transcript/list.txt
rm ../transcript/list.closed.txt

cp -a ../adaptation/* .

echo "=================="
echo "Generating adapted language model..."

pocketsphinx_mdef_convert -text $lang/mdef $lang/mdef.txt
sphinx_fe -argfile $lang/feat.params \
        -samprate 16000 -c list.fileids \
       -di . -do . -ei wav -eo mfc -mswav yes

/usr/lib/sphinxtrain/bw \
 -hmmdir $lang \
 -moddeffn $lang/mdef.txt \
 -ts2cbfn .ptm. \
 -feat 1s_c_d_dd \
 -svspec 0-12/13-25/26-38 \
 -cmn current \
 -agc none \
 -dictfn cmudict-$lang.dict \
 -ctlfn list.fileids \
 -lsnfn list.transcript \
 -accumdir .

 /usr/lib/sphinxtrain/mllr_solve \
    -meanfn $lang/means \
    -varfn $lang/variances \
    -outmllrfn mllr_matrix -accumdir .

rm -rf $lang-adapt
cp -a $lang $lang-adapt

/usr/lib/sphinxtrain/map_adapt \
    -moddeffn $lang/mdef.txt \
    -ts2cbfn .ptm. \
    -meanfn $lang/means \
    -varfn $lang/variances \
    -mixwfn $lang/mixture_weights \
    -tmatfn $lang/transition_matrices \
    -accumdir . \
    -mapmeanfn $lang-adapt/means \
    -mapvarfn $lang-adapt/variances \
    -mapmixwfn $lang-adapt/mixture_weights \
    -maptmatfn $lang-adapt/transition_matrices



/usr/lib/sphinxtrain/mk_s2sendump \
    -pocketsphinx yes \
    -moddeffn $lang-adapt/mdef.txt \
    -mixwfn $lang-adapt/mixture_weights \
    -sendumpfn $lang-adapt/sendump

text2wfreq < list.txt | wfreq2vocab > list.tmp.vocab

text2idngram -vocab list.tmp.vocab -idngram list.idngram < list.closed.txt
idngram2lm -vocab_type 0 -idngram list.idngram -vocab list.tmp.vocab -arpa list.lm

sphinx_lm_convert -i list.lm -o list.lm.bin

# Cleanup
rm -rf en-us en-us.lm.bin *mfc gauden_counts list.idngram list.tmp.vocab list.transcript list.fileids list.txt list.closed.txt mixw_counts mllr_matrix gauden_counts tmat_counts

echo "=================="
echo "Generated adapted language model!"
echo "=================="
echo "Testing adapted language model..."

for f in *wav; do
    newtxt=new$(basename $f .wav).txt
    echo -en "\r\033[KTesting $f...";
    pocketsphinx_continuous -lm list.lm.bin -hmm en-us-adapt/ -infile $f 2>/dev/null > $newtxt
done

# Fix formatting of transcriptions
../fixTranscripts

for newtxt in new*txt; do
    echo "For $newtxt: "
    oldtxt=${newtxt/new/}
    diff --color $oldtxt $newtxt && echo "OK!"
done

rm *wav # *txt

mkdir -p tests-output
mv *txt tests-output
