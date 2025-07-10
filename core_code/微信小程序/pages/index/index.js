// index.js
Page({
  data: {
    rescueCount: 23,
    // 定义页面映射关系，便于维护
    pageMap: {
      'patrol': '/pages/patrol/patrol',
      'education': '/pages/education/education',
      'history': '/pages/history/history'
    }
  },
  
  // 导航到功能页面
  navigateToFunction(e) {
    const type = e.currentTarget.dataset.type || '';
    const targetPage = this.data.pageMap[type];
    
    // 参数验证
    if (!targetPage) {
      console.warn(`无效的跳转类型: ${type}`);
      // 兜底跳转
      wx.navigateTo({ url: '/pages/function/function?type=' + type });
      return;
    }
    
    // 执行跳转
    wx.navigateTo({ url: targetPage });
  }
})