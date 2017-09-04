# .bashrc

DEBEMAIL="tahazayed@gmail.com"
DEBFULLNAME="Taha Amin"
VERSION=$( bash <<EOF
bin/Release/SKDownloader -v
EOF
)
BUILDDATE=$( bash <<EOF
bin/Release/SKDownloader -b
EOF
)
export DEBEMAIL DEBFULLNAME VERSION BUILDDATE


