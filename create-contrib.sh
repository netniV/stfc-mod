#!env /bin/bash
git shortlog -sne --all | awk '{$1=""; print "- " substr($0,2)}' > CONTRIBUTORS.md
