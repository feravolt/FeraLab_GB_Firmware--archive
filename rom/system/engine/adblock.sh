#!/system/xbin/sh
### FeraDroid Engine v18.1 | By FeraVolt. 2015

pm list packages | tr -d '\r' | grep -v 'package:android' | grep -v 'com.android.' | grep -v 'com.mstar.android.' | grep -v 'com.konka.' | while read PACKAGE; do
    PACKAGE=${PACKAGE/package:/}
    echo $PACKAGE
    pm disable $PACKAGE/com.google.ads.AdView
    pm disable $PACKAGE/com.google.ads.InterstitialAd
    pm disable $PACKAGE/com.google.android.gms.ads.AdView
    pm disable $PACKAGE/com.google.android.gms.ads.InterstitialAd
    pm disable $PACKAGE/com.google.android.gms.ads.doubleclick.PublisherAdView
    pm disable $PACKAGE/com.google.android.gms.ads.doubleclick.PublisherInterstitialAd
    pm disable $PACKAGE/com.google.android.gms.ads.search.SearchAdView
    pm disable $PACKAGE/jp.tjkapp.adfurikunsdk.AdfurikunBase
    pm disable $PACKAGE/com.admarvel.android.ads.AdMarvelView
    pm disable $PACKAGE/com.admarvel.android.ads.AdMarvelInterstitialAds
    pm disable $PACKAGE/com.amazon.device.ads.AdLayout
    pm disable $PACKAGE/com.amobee.adsdk.AdManager
    pm disable $PACKAGE/com.appbrain.AppBrainBanner
    pm disable $PACKAGE/com.appbrain.AppBrain
    pm disable $PACKAGE/com.bonzai.view.BonzaiAdView
    pm disable $PACKAGE/com.chartboost.sdk.Chartboost
    pm disable $PACKAGE/cn.domob.android.ads.DomobAdView
    pm disable $PACKAGE/cn.domob.android.ads.DomobInterstitialAd
    pm disable $PACKAGE/com.flurry.android.FlurryAds
    pm disable $PACKAGE/com.hodo.HodoADView
    pm disable $PACKAGE/com.inmobi.monetization.IMBanner
    pm disable $PACKAGE/com.inmobi.monetization.IMInterstitial
    pm disable $PACKAGE/com.waystorm.ads.WSAdBanner
    pm disable $PACKAGE/com.adsdk.sdk.banner.InAppWebView
    pm disable $PACKAGE/de.madvertise.android.sdk.MadvertiseMraidView
    pm disable $PACKAGE/mediba.ad.sdk.android.openx.MasAdView
    pm disable $PACKAGE/com.mdotm.android.view.MdotMAdView
    pm disable $PACKAGE/com.mdotm.android.view.MdotMInterstitial
    pm disable $PACKAGE/com.millennialmedia.android.MMAdView
    pm disable $PACKAGE/com.millennialmedia.android.MMInterstitial
    pm disable $PACKAGE/com.mobclix.android.sdk.MobclixMMABannerXLAdView
    pm disable $PACKAGE/com.mopub.mobileads.MoPubView
    pm disable $PACKAGE/com.mopub.mobileads.MraidBanner
    pm disable $PACKAGE/net.nend.android.NendAdView
    pm disable $PACKAGE/com.og.wa.AdWebView
    pm disable $PACKAGE/com.onelouder.adlib.AdView
    pm disable $PACKAGE/com.onelouder.adlib.AdInterstitial
    pm disable $PACKAGE/com.openx.ad.mobile.sdk.interfaces.OXMAdBannerView
    pm disable $PACKAGE/com.smartadserver.android.library.ui.SASAdView
    pm disable $PACKAGE/com.startapp.android.publish.HtmlAd
    pm disable $PACKAGE/com.tapfortap.AdView
    pm disable $PACKAGE/com.tapfortap.Interstitial
    pm disable $PACKAGE/com.taiwanmobile.pt.adp.view.TWMAdView
    pm disable $PACKAGE/com.taiwanmobile.pt.adp.view.TWMAdViewInterstitial
    pm disable $PACKAGE/com.vpadn.ads.VpadnBanner
    pm disable $PACKAGE/com.vpadn.ads.VpadnInterstitialAd
    pm disable $PACKAGE/com.vpon.ads.VponBanner
    pm disable $PACKAGE/com.vpon.ads.VponInterstitialAd
done
