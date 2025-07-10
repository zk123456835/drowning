// pages/education/education.js
Page({
  data: {
    videoSrc: "https://example.com/safety.mp4",
    videoLoading: true
  },
  
  onVideoLoadStart() {
    this.setData({ videoLoading: true });
  },
  
  onVideoLoaded() {
    this.setData({ videoLoading: false });
  },
  
  goBack() {
    wx.navigateBack();
  }
});