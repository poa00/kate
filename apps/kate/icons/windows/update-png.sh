for i in 16 22 32 44 48 64 128 150 256 310 512; do
    ksvgtopng5 $i $i sc-apps-kate.svg $i-apps-kate.png
    # BUG: 442761, see https://docs.microsoft.com/en-us/windows/msix/desktop/desktop-to-uwp-manual-conversion#optional-add-target-based-unplated-assets
    cp $i-apps-kate.png $i-apps-kate.targetsize-${i}_altform-unplated.png
done
