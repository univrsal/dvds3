#!/bin/sh
# Automatic packaging for linux builds

version=$1
data_dir="../data"
project="dvd-screensaver"
arch="ubuntu.x64"
build_location="../build"
build_dir=$project.v$version.$arch

if [ -z "$version" ]; then
	echo "Please provide a version string"
	exit
fi

# Build 
cd $build_location
cmake ..
make
cd "../package"

echo "Creating build directory"
mkdir -p $build_dir/$project/bin/64bit

echo "Fetching build from $build_location"
cp $build_location/$project.so $build_dir/$project/bin/64bit/

echo "Fetching locale from $data_dir"
cp -R $data_dir $build_dir/$project

echo "Fetching misc files"
cp ../COPYING $build_dir/LICENSE.txt
cp ./README.txt $build_dir/README.txt

echo "Writing version number $version"
sed -i -e "s/@VERSION/$version/g" $build_dir/README.txt

echo "Zipping to $project.v$version.$arch.zip"
cd $build_dir
zip -r "../$project.v$version.$arch.zip" ./ 
cd ..

echo "Cleaning up"
rm -rf $build_dir
