#!/usr/bin/env bash
set -e

echo "Applying iOS patches to submodules..."

# Apply JUCE patch
if cd libs/JUCE && git apply --check ../../juce_ios.patch >/dev/null 2>&1; then
    git apply ../../juce_ios.patch
    echo "✅ Applied juce_ios.patch to libs/JUCE"
else
    echo "⚠️  juce_ios.patch already applied or conflicts in libs/JUCE"
fi
cd ../..

# Apply sst-plugininfra patch
if cd libs/sst/sst-plugininfra && git apply --check ../../../sst_plugininfra_ios.patch >/dev/null 2>&1; then
    git apply ../../../sst_plugininfra_ios.patch
    echo "✅ Applied sst_plugininfra_ios.patch to libs/sst/sst-plugininfra"
else
    echo "⚠️  sst_plugininfra_ios.patch already applied or conflicts in libs/sst/sst-plugininfra"
fi
cd ../../..

echo "Done!"
