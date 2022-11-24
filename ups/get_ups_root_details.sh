if [ ${#} -gt 0 ] && [ "${1}" == "-?" ]; then
  echo "get_ups_root_details.sh [-q -v]"
  echo "  -q : Say ROOT qualifiers from ups active"
  echo "  -v : Say ROOT version from ups active"
  exit 0
fi

if ! hash ups &> /dev/null; then
  echo "[ERROR]: ${0}: ups command is not available" >&2
  exit 1
fi

ups active | grep -q "^root"

if [ "${?}" != "0" ]; then
  echo "[ERROR]: ${0}: root package is not active in ups:" >&2
  ups active
  exit 1
fi

VERSION=$(ups active | grep "^root" | sed "s:\s\+: :g" | cut -d " " -f 2)
QUALS=$(ups active | grep "^root" | sed "s:\s\+: :g" | cut -d " " -f 6)

if [ ${#} -lt 1 ]; then
  echo "${VERSION} ${QUALS}"
  exit 0
elif [ "${1}" == "-v" ]; then
  echo "${VERSION}"
  exit 0
elif [ "${1}" == "-q" ]; then
  echo "${QUALS}"
  exit 0
fi

exit 1