#!/bin/bash

# DemiEngine Changelog Generator
# Generates changelog from git history with version tracking

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VERSION_FILE="${SCRIPT_DIR}/VERSION"
CHANGELOG_FILE="${SCRIPT_DIR}/CHANGELOG.md"

# Function to print output without colors
print_status() {
	echo "[INFO] $1"
}

print_warning() {
	echo "[WARN] $1"
}

print_error() {
	echo "[ERROR] $1"
}

print_header() {
	echo "[CHANGELOG] $1"
}

# Function to get current version
get_current_version() {
	if [[ -f ${VERSION_FILE} ]]; then
		cat "${VERSION_FILE}"
	else
		echo "0.1.0"
	fi
}

# Function to increment version
increment_version() {
	local version=$1
	local type=$2

	IFS='.' read -ra VERSION_PARTS <<<"${version}"
	local major=${VERSION_PARTS[0]}
	local minor=${VERSION_PARTS[1]}
	local patch=${VERSION_PARTS[2]}

	case ${type} in
	"major")
		major=$((major + 1))
		minor=0
		patch=0
		;;
	"minor")
		minor=$((minor + 1))
		patch=0
		;;
	"patch")
		patch=$((patch + 1))
		;;
	*)
		print_error "Invalid version type. Use: major, minor, or patch"
		exit 1
		;;
	esac

	echo "${major}.${minor}.${patch}"
}

# Function to categorize commits
categorize_commit() {
	local message=$1

	# Check for conventional commit patterns
	if [[ ${message} =~ ^feat(\(.*\))?:.*$ ]]; then
		echo "✨ Features"
	elif [[ ${message} =~ ^fix(\(.*\))?:.*$ ]]; then
		echo "🐛 Bug Fixes"
	elif [[ ${message} =~ ^docs(\(.*\))?:.*$ ]]; then
		echo "📚 Documentation"
	elif [[ ${message} =~ ^style(\(.*\))?:.*$ ]]; then
		echo "💅 Code Style"
	elif [[ ${message} =~ ^refactor(\(.*\))?:.*$ ]]; then
		echo "♻️ Code Refactoring"
	elif [[ ${message} =~ ^perf(\(.*\))?:.*$ ]]; then
		echo "⚡ Performance Improvements"
	elif [[ ${message} =~ ^test(\(.*\))?:.*$ ]]; then
		echo "🧪 Tests"
	elif [[ ${message} =~ ^build(\(.*\))?:.*$ ]]; then
		echo "🔧 Build System"
	elif [[ ${message} =~ ^ci(\(.*\))?:.*$ ]]; then
		echo "👷 Continuous Integration"
	elif [[ ${message} =~ ^chore(\(.*\))?:.*$ ]]; then
		echo "🔨 Chores"
	elif [[ ${message} =~ ^revert(\(.*\))?:.*$ ]]; then
		echo "⏪ Reverts"
	# Custom categories for DemiEngine
	elif [[ ${message} =~ [Ff]ix.*argument.*parsing ]]; then
		echo "🐛 Bug Fixes"
	elif [[ ${message} =~ [Aa]dd.*SIMD ]]; then
		echo "✨ Features"
	elif [[ ${message} =~ [Ii]mplement.*preprocessor ]]; then
		echo "✨ Features"
	elif [[ ${message} =~ [Ee]nhance.*test ]]; then
		echo "🧪 Tests"
	elif [[ ${message} =~ [Aa]dd.*test ]]; then
		echo "🧪 Tests"
	elif [[ ${message} =~ [Cc]lean.*up ]]; then
		echo "🔨 Chores"
	elif [[ ${message} =~ [Uu]pdate.*documentation ]]; then
		echo "📚 Documentation"
	elif [[ ${message} =~ [Aa]dd.*opcode ]]; then
		echo "✨ Features"
	elif [[ ${message} =~ [Ee]nhance.*CPU ]]; then
		echo "⚡ Performance Improvements"
	else
		echo "🔄 Other Changes"
	fi
}

# Function to generate changelog for a version
generate_version_changelog() {
	local version=$1
	local since_commit=$2
	local until_commit=${3:-"HEAD"}

	# Get commit range
	local commit_range
	if [[ -n ${since_commit} ]]; then
		commit_range="${since_commit}..${until_commit}"
	else
		commit_range="${until_commit}"
	fi

	# Get commits with details (disable color output and ensure UTF-8)
	local commits
	commits=$(git -c color.ui=never -c core.quotePath=false log "${commit_range}" --pretty=format:"%H|%s|%an|%ad" --date=short --reverse)

	if [[ -z ${commits} ]]; then
		print_warning "No commits found in range ${commit_range}"
		return
	fi

	# Group commits by category
	declare -A categories

	while IFS='|' read -r hash message author date; do
		local category
		category=$(categorize_commit "${message}")

		if [[ -z ${categories[${category}]} ]]; then
			categories["${category}"]=""
		fi

		# Clean up message (remove conventional commit prefixes)
		local clean_message
		clean_message=$(echo "${message}" | sed -E 's/^(feat|fix|docs|style|refactor|perf|test|build|ci|chore|revert)(\([^)]+\))?: //')

		categories["${category}"]+="- ${clean_message} (${hash:0:7}) - ${author}\n"
	done <<<"${commits}"

	# Generate changelog entry
	local changelog_entry
	changelog_entry="## [${version}] - $(date '+%Y-%m-%d')\n\n"

	# Sort categories in a logical order
	local ordered_categories=(
		"✨ Features"
		"⚡ Performance Improvements"
		"🐛 Bug Fixes"
		"🧪 Tests"
		"📚 Documentation"
		"♻️ Code Refactoring"
		"💅 Code Style"
		"🔧 Build System"
		"👷 Continuous Integration"
		"🔨 Chores"
		"⏪ Reverts"
		"🔄 Other Changes"
	)

	for category in "${ordered_categories[@]}"; do
		if [[ -n ${categories[${category}]} ]]; then
			changelog_entry+="### ${category}\n\n"
			changelog_entry+="${categories[${category}]}\n"
		fi
	done

	echo -e "${changelog_entry}"
}

# Function to update VERSION file
update_version_file() {
	local version=$1
	echo "${version}" >"${VERSION_FILE}"
	print_status "Updated VERSION file to ${version}"
}

# Function to update src/config.hpp with new version
update_config_hpp() {
	local version=$1
	local config_file="${SCRIPT_DIR}/src/config.hpp"

	if [[ -f ${config_file} ]]; then
		# Extract major.minor.patch
		IFS='.' read -ra VERSION_PARTS <<<"${version}"
		local major=${VERSION_PARTS[0]}
		local minor=${VERSION_PARTS[1]}
		local patch=${VERSION_PARTS[2]}

		# Update version constants in config.hpp
		sed -i.bak \
			-e "s/DEMI_VERSION_MAJOR [0-9]*/DEMI_VERSION_MAJOR ${major}/" \
			-e "s/DEMI_VERSION_MINOR [0-9]*/DEMI_VERSION_MINOR ${minor}/" \
			-e "s/DEMI_VERSION_PATCH [0-9]*/DEMI_VERSION_PATCH ${patch}/" \
			-e "s/DEMI_VERSION_STRING \"[^\"]*\"/DEMI_VERSION_STRING \"${version}\"/" \
			"${config_file}"

		rm -f "${config_file}.bak"
		print_status "Updated ${config_file} with version ${version}"
	else
		print_warning "Config file not found: ${config_file}"
	fi
}

# Function to create a git tag
create_git_tag() {
	local version=$1
	local tag_name="v${version}"

	git tag -a "${tag_name}" -m "Release version ${version}"
	print_status "Created git tag: ${tag_name}"
}

# Main function
main() {
	local command=${1:-"generate"}

	case ${command} in
	"generate")
		local since_tag=${2:-""}
		local version
		version=$(get_current_version)

		print_status "Generating changelog for current version: ${version}"

		if [[ -n ${since_tag} ]]; then
			generate_version_changelog "${version}" "${since_tag}" >>"${CHANGELOG_FILE}.tmp"
		else
			# Generate full changelog
			echo "# Changelog" >"${CHANGELOG_FILE}.tmp"
			echo "" >>"${CHANGELOG_FILE}.tmp"
			echo "All notable changes to DemiEngine will be documented in this file." >>"${CHANGELOG_FILE}.tmp"
			echo "" >>"${CHANGELOG_FILE}.tmp"
			echo "The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/)," >>"${CHANGELOG_FILE}.tmp"
			echo "and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html)." >>"${CHANGELOG_FILE}.tmp"
			echo "" >>"${CHANGELOG_FILE}.tmp"

			generate_version_changelog "${version}" "" >>"${CHANGELOG_FILE}.tmp"
		fi

		mv "${CHANGELOG_FILE}.tmp" "${CHANGELOG_FILE}"
		print_status "Changelog written to ${CHANGELOG_FILE}"
		;;

	"release")
		local version_type=${2:-"patch"}
		local current_version
		current_version=$(get_current_version)
		local new_version
		new_version=$(increment_version "${current_version}" "${version_type}")

		print_status "Creating release: ${current_version} -> ${new_version}"

		# Update version files
		update_version_file "${new_version}"
		update_config_hpp "${new_version}"

		# Generate changelog for this release
		local last_tag
		last_tag=$(git describe --tags --abbrev=0 2>/dev/null || echo "")

		if [[ -n ${last_tag} ]]; then
			print_status "Generating changelog since last tag: ${last_tag}"
			generate_version_changelog "${new_version}" "${last_tag}" >"${CHANGELOG_FILE}.tmp"
		else
			print_status "No previous tags found, generating full changelog"
			generate_version_changelog "${new_version}" "" >"${CHANGELOG_FILE}.tmp"
		fi

		# Prepend to existing changelog
		if [[ -f ${CHANGELOG_FILE} ]]; then
			echo "" >>"${CHANGELOG_FILE}.tmp"
			cat "${CHANGELOG_FILE}" >>"${CHANGELOG_FILE}.tmp"
		fi
		mv "${CHANGELOG_FILE}.tmp" "${CHANGELOG_FILE}"

		# Commit version changes
		git add "${VERSION_FILE}" "${CHANGELOG_FILE}" src/config.hpp 2>/dev/null || true
		git commit -m "chore: bump version to ${new_version}"

		# Create tag
		create_git_tag "${new_version}"

		print_status "Release ${new_version} created successfully!"
		;;

	"version")
		local current_version
		current_version=$(get_current_version)
		echo "Current version: ${current_version}"
		;;

	"help" | "-h" | "--help")
		echo "DemiEngine Changelog Generator"
		echo ""
		echo "Usage: $0 <command> [options]"
		echo ""
		echo "Commands:"
		echo "  generate [since_tag]  Generate changelog (default: full history)"
		echo "  release <type>        Create new release (type: major|minor|patch)"
		echo "  version               Show current version"
		echo "  help                  Show this help"
		echo ""
		echo "Examples:"
		echo "  $0 generate           # Generate full changelog"
		echo "  $0 generate v1.0.0    # Generate changelog since v1.0.0"
		echo "  $0 release patch      # Create patch release (e.g., 1.0.0 -> 1.0.1)"
		echo "  $0 release minor      # Create minor release (e.g., 1.0.0 -> 1.1.0)"
		echo "  $0 release major      # Create major release (e.g., 1.0.0 -> 2.0.0)"
		;;

	*)
		print_error "Unknown command: ${command}"
		echo "Use '$0 help' for usage information"
		exit 1
		;;
	esac
}

# Check if we're in a git repository
if ! git rev-parse --git-dir >/dev/null 2>&1; then
	print_error "This script must be run from within a git repository"
	exit 1
fi

main "$@"
