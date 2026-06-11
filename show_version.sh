#!/bin/bash

# DemiEngine Version Display Script
# Shows current version information from multiple sources

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VERSION_FILE="${SCRIPT_DIR}/VERSION"
CONFIG_FILE="${SCRIPT_DIR}/src/config.hpp"

# Colors
BLUE='\033[0;34m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${BLUE}DemiEngine Version Information${NC}"
echo "================================"

# Version from VERSION file
if [[ -f ${VERSION_FILE} ]]; then
	VERSION=$(cat "${VERSION_FILE}")
	echo -e "${GREEN}Version File:${NC} ${VERSION}"
else
	echo -e "${YELLOW}Version File:${NC} Not found"
fi

# Version from config.hpp
if [[ -f ${CONFIG_FILE} ]]; then
	MAJOR=$(grep "#define DEMI_VERSION_MAJOR" "${CONFIG_FILE}" | awk '{print $3}' | tr -d '\r')
	MINOR=$(grep "#define DEMI_VERSION_MINOR" "${CONFIG_FILE}" | awk '{print $3}' | tr -d '\r')
	PATCH=$(grep "#define DEMI_VERSION_PATCH" "${CONFIG_FILE}" | awk '{print $3}' | tr -d '\r')
	VERSION_STRING=$(grep "#define DEMI_VERSION_STRING" "${CONFIG_FILE}" | sed 's/.*"\([^"]*\)".*/\1/' | tr -d '\r')

	if [[ -n ${MAJOR} && -n ${MINOR} && -n ${PATCH} ]]; then
		echo -e "${GREEN}Config Header:${NC} ${MAJOR}.${MINOR}.${PATCH}"
	else
		echo -e "${YELLOW}Config Header:${NC} Parse error"
	fi

	if [[ -n ${VERSION_STRING} ]]; then
		echo -e "${GREEN}Version String:${NC} ${VERSION_STRING}"
	else
		echo -e "${YELLOW}Version String:${NC} Parse error"
	fi
else
	echo -e "${YELLOW}Config Header:${NC} Not found"
fi

# Git information
if git rev-parse --git-dir >/dev/null 2>&1; then
	CURRENT_COMMIT=$(git rev-parse --short HEAD)
	CURRENT_BRANCH=$(git branch --show-current)

	# Check for uncommitted changes
	if git diff-index --quiet HEAD --; then
		STATUS_CLEAN="✓"
	else
		STATUS_CLEAN="⚠ (uncommitted changes)"
	fi

	echo -e "${GREEN}Git Commit:${NC} ${CURRENT_COMMIT} (${CURRENT_BRANCH}) ${STATUS_CLEAN}"

	# Latest tag
	LATEST_TAG=$(git describe --tags --abbrev=0 2>/dev/null || echo "No tags")
	echo -e "${GREEN}Latest Tag:${NC} ${LATEST_TAG}"

	# Commits since tag
	if [[ ${LATEST_TAG} != "No tags" ]]; then
		COMMITS_SINCE=$(git rev-list "${LATEST_TAG}..HEAD" --count)
		if [[ ${COMMITS_SINCE} -gt 0 ]]; then
			echo -e "${GREEN}Commits since tag:${NC} ${COMMITS_SINCE}"
		fi
	fi
fi

# Build date (if available)
if [[ -f "${SCRIPT_DIR}/bin/demi-engine" ]]; then
	BUILD_DATE=$(stat -c %y "${SCRIPT_DIR}/bin/demi-engine" 2>/dev/null | cut -d' ' -f1)
	echo -e "${GREEN}Binary Build Date:${NC} ${BUILD_DATE}"
fi

echo ""
echo -e "${BLUE}Feature Flags:${NC}"

# Check feature flags from config.hpp
if [[ -f ${CONFIG_FILE} ]]; then
	check_feature() {
		local feature=$1
		local name=$2
		if grep -q "#define ${feature} 1" "${CONFIG_FILE}"; then
			echo -e "  ${GREEN}✓${NC${ $na}me"
		else
			echo -e "  ${YELLOW}✗${NC${ $na}me"
		fi
	}

	check_feature "DEMI_SIMD_SUPPORT" "SIMD Support"
	check_feature "DEMI_PREPROCESSOR_SUPPORT" "Preprocessor Support"
	check_feature "DEMI_INCLUDE_SUPPORT" "Include Support"
	check_feature "DEMI_FUSION_SUPPORT" "Instruction Fusion"
fi

echo ""
