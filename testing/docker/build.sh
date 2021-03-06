#! /bin/bash -e

# This file is a wrapper around docker build with specific concerns around image
# versions and registry deployment... It also attempts to detect any potential
# missing dependencies and warns you about them.

gecko_root="$( cd "$( dirname "${BASH_SOURCE[0]}" )/../.." && pwd )"

usage() {
  echo "Build a docker image (and tag it)"
  echo
  echo "$0 <image-name>"
  echo
  echo "  Images are defined in testing/docker/<image-name>."
  echo "  For more see: $PWD/README.md"
  echo
}

usage_err() {
  echo $1
  echo
  usage
  exit 1
}

find_registry() {
  local reg="$1/REGISTRY"

  if [ -f $reg ];
  then
    echo $folder
    return
  fi
}

build() {
  local image_name=$1
  local folder="$gecko_root/testing/docker/$image_name"
  local folder_reg="$folder/REGISTRY"
  local folder_ver="$folder/VERSION"

  if [ "$image_name" == "" ];
  then
    usage
    return
  fi

  test -d "$folder" || usage_err "Unknown image: $image_name"
  test -f "$folder_ver" || usage_err "$folder must contain VERSION file"

  # Fallback to default registry if one is not in the folder...
  if [ ! -f "$folder_reg" ]; then
    folder_reg=$PWD/REGISTRY
  fi

  local registry=$(cat $folder_reg)
  local version=$(cat $folder_ver)

  test -n "$registry" || usage_err "$folder_reg is empty aborting..."
  test -n "$version" || usage_err "$folder_ver is empty aborting..."

  local tag="$registry/$image_name:$version"

  if [ -f $folder/build.sh ]; then
    shift
    $folder/build.sh -t $tag $* || exit 1
  else
    # use --no-cache so that we always get the latest updates from yum
    # and use the latest version of system-setup.sh
    ( cd $folder/.. && docker build --no-cache -t $tag $image_name ) || exit 1
  fi

  echo "Success built $image_name and tagged with $tag"
  echo "If deploying now you can run 'docker push $tag'"
}

if ! which docker > /dev/null; then
  echo "Docker must be installed read installation instructions at docker.com"
  echo
  usage
  exit 1
fi

# TODO: In the future we should check minimum docker version it does matter.
if ! docker version > /dev/null;
then
  echo "Docker server is unresponsive run 'docker ps' and check that docker is"
  echo "running"
  echo
  usage
  exit 1
fi

build $*
