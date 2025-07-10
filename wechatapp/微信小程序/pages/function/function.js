Page({
  data: {
    functionName: '功能页面'
  },
  
  onLoad(options) {
    // 根据传递的参数设置功能名称
    const nameMap = {
      patrol: '巡查地点',
      education: '教育培训',
      history: '历史数据'
    };
    
    this.setData({
      functionName: nameMap[options.type] || '功能页面'
    });
    
    // 设置导航栏标题
    wx.setNavigationBarTitle({
      title: this.data.functionName
    })
  },
  
  // 返回首页
  goBack() {
    wx.navigateBack()
  }
})