## Common code to be used by all scripts
##
if ! [ -x "$(command -v unifdef)" ]; then
  echo 'Error: unifdef is not installed.' >&2
  exit 1
fi

export IFS=" "

generate_driver()
{
	NEW_DIR="$PWD/$PROJECT"
	ENABLE=$1
	DISABLE=$2

	#echo "Enable is $ENABLE"
	DEFINES=""
	for define in $ENABLE; do
	  DEFINES=$DEFINES" -D$define"
	done
	echo "Enabling $DEFINES"

	#echo "Disable is $DISABLE"
	UNDEFINES=""
	for undefine in $DISABLE; do
	  UNDEFINES=$UNDEFINES" -U$undefine"
	done
	echo "Disabling $UNDEFINES"

	rm -rf $NEW_DIR
	find . -name '*.[ch]' | while read f
	do
		mkdir -p `dirname $NEW_DIR/$f`
		touch $f
		echo "Processing file $f new: $NEW_DIR/$f ..."
		unifdef $DEFINES $UNDEFINES $f> $NEW_DIR/$f
	done
}
