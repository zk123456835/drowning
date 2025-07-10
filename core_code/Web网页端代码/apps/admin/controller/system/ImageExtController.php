<?php
namespace app\admin\controller\system;

use app\admin\model\content\CompanyModel;
use app\admin\model\content\ContentModel;
use app\admin\model\content\ContentSortModel;
use app\admin\model\content\LinkModel;
use app\admin\model\content\SiteModel;
use app\admin\model\content\SlideModel;
use app\home\model\MemberModel;
use core\basic\Controller;

class ImageExtController extends Controller
{
    private $companyModel;
    private $contentSortModel;
    private $contentModel;
    private $linkModel;
    private $memberModel;
    private $siteModel;
    private $slideModel;

    public function __construct()
    {
        $this->companyModel = new CompanyModel();
        $this->contentSortModel = new ContentSortModel();
        $this->contentModel = new ContentModel();
        $this->linkModel = new LinkModel();
        $this->memberModel = new MemberModel();
        $this->siteModel = new SiteModel();
        $this->slideModel = new SlideModel();
    }

    public function index(){
        $this->display('system/extimage.html');
    }

    public function checkUploadFileCount(){
        $Filepath = DOC_PATH . STATIC_DIR . '/upload';
        $fileList = get_dir($Filepath);
        $fileArr = [];
        array_walk_recursive($fileList,function ($key) use (&$fileArr){
            $fileArr[] = $key;
            return $fileArr;
        });
        return json(1,['count' => count($fileArr)]);
    }

    public function checkDataFileCount(){
        $dataArr = [];
        $company = $this->companyModel->getImage();
        $contentSort = $this->contentSortModel->getImage();
        $content = $this->contentModel->getImage();
        $link = $this->linkModel->getImage();
        $member = $this->memberModel->getImage();
        $site = $this->siteModel->getImage();
        $slide = $this->slideModel->getImage();
        $resArr = array_merge_recursive($company,$contentSort,$content,$link,$member,$site,$slide);
        array_walk_recursive($resArr,function ($key1) use (&$dataArr){
            if(!empty($key1)){
                $dataArr[] = DOC_PATH . $key1;
            }
            return $dataArr;
        });
        return json(1,['count' => count($dataArr)]);

    }

    public function do_ext(){
        //获取上传的文件
        $Filepath = DOC_PATH . STATIC_DIR . '/upload';
        $fileList = get_dir($Filepath);
        $fileArr = [];
        array_walk_recursive($fileList,function ($key) use (&$fileArr){
            $fileArr[] = $key;
            return $fileArr;
        });
        //查询数据
        $dataArr = [];
        $company = $this->companyModel->getImage();
        $contentSort = $this->contentSortModel->getImage();
        $content = $this->contentModel->getImage();
        $link = $this->linkModel->getImage();
        $member = $this->memberModel->getImage();
        $site = $this->siteModel->getImage();
        $slide = $this->slideModel->getImage();
        $resArr = array_merge_recursive($company,$contentSort,$content,$link,$member,$site,$slide);
        array_walk_recursive($resArr,function ($key1) use (&$dataArr){
            if(!empty($key1)){
                $dataArr[] = DOC_PATH . $key1;
            }
            return $dataArr;
        });
        $dataArr = array_unique($dataArr);
        //对比文件并执行文件迁移
        $difference = array_diff($fileArr,$dataArr);
        $movePath = DOC_PATH . STATIC_DIR . '/backup/ImageExt/';
        check_dir($movePath,true);
        foreach ($difference as $path){
            $fileName = substr(strrchr($path, "/"), 1);
            rename($path ,$movePath . $fileName);
        }
        return json(1,['count' => count($difference)]);
    }
}
