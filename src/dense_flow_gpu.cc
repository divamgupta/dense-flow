#include "dense_flow_gpu.hh"


// ./denseFlow_gpu --inFramesList="/mnt/server3/home/divam14038/ego_multitask/data/raw/GTEA/pngList.list" --xFrameLists="/mnt/server3/home/divam14038/ego_multitask/data/raw/GTEA/flow_x.list" --yFrameLists="/mnt/server3/home/divam14038/ego_multitask/data/raw/GTEA/flow_y.list"


#include <fstream>


// Compute optical flow between frames t and t+steppings[*]
const std::vector< int64_t > flow_span { 1 };

int resize_first , resize_first_width ,  resize_first_height ;

cv::Ptr<cv::cuda::FarnebackOpticalFlow> alg_farn;
cv::Ptr<cv::cuda::OpticalFlowDual_TVL1> alg_tvl1;
cv::Ptr<cv::cuda::BroxOpticalFlow> alg_brox;

int main(int argc, char** argv){
  bool serialize = false;
  const int64_t max_files_chunk = MAX_FILES_PER_CHUNK;

  #ifdef SERIALIZE_BUFFER
  serialize = false;
  #endif 
  
  std::cout << "kkkkkkk \n " ; 
  // IO operation
  const cv::String keys =
  "{ f inFramesList     | | filename of video }"
  "{ i imgFile     | | filename of image component }"
  "{ x xFrameLists   | | filename of flow x component }"
  "{ y yFrameLists   | | filename of flow x component }"
  "{ b bound       | 15 | specify the maximum of optical flow}"
  "{ t type        | 0 | specify the optical flow algorithm }"
  "{ d device_id   | 0 | set gpu id }"
  "{ s step        | 1 | specify the step for frame sampling}"
  "{ o offset      | 0 | specify the offset from where to start}"
  "{ c clip        | 0 | specify maximum length of clip (0=no maximum)}"
  "{ r resize_first        | 0 | 1 if you have to resize the cv image then compute flow and 0 if u wanna resie after calculating the flow }"
  "{ r resize_first_width        | 0 | resize first width }"
  "{ r resize_first_height        | 0 | resize first height }"

  ;

  cv::CommandLineParser cmd(argc, argv, keys);

  std::string inFramesList     = cmd.get<std::string>("inFramesList");
  std::string imgFile     = cmd.get<std::string>("imgFile");
  std::string xFrameLists   = cmd.get<std::string>("xFrameLists");
  std::string yFrameLists   = cmd.get<std::string>("yFrameLists");
  int bound               = cmd.get<int>("bound");
  int type                = cmd.get<int>("type");
  int device_id           = cmd.get<int>("device_id");
  int step                = cmd.get<int>("step");
  int offset              = cmd.get<int>("offset");
  int len_clip            = cmd.get<int>("clip");
  resize_first            = cmd.get<int>("resize_first");
  resize_first_width            = cmd.get<int>("resize_first_width");
  resize_first_height            = cmd.get<int>("resize_first_height");
  
  if( resize_first_height <= 0 )
    resize_first_height = DIM_Y ;
  if( resize_first_width <= 0 )
    resize_first_height = DIM_X ;

  if( !cmd.check() ){
    cmd.printErrors();
    return EXIT_FAILURE;
  }

  cv::cuda::setDevice( device_id );
  

  

  switch( type ){
    case 0:
      alg_farn = cv::cuda::FarnebackOpticalFlow::create();
      break;
    case 1:
      alg_tvl1 = cv::cuda::OpticalFlowDual_TVL1::create();
      break;
    case 2:
      alg_brox = cv::cuda::BroxOpticalFlow::create(0.197f, 50.0f, 0.8f, 10, 77, 10);
      break;
    default:
      alg_brox = cv::cuda::BroxOpticalFlow::create(0.197f, 50.0f, 0.8f, 10, 77, 10);
  }

  //Video v( inFramesList, step, len_clip );    // Sample every `step`-th frame, up to the `len_clip`-th frame, i.e., floor(len_clip/step) frames total.

  // if( !v.is_open() )
  //   return EXIT_FAILURE;

  // if( offset > v.length() ){
  //   std::cerr << "Offset exceeds length of video." << std::endl;
  //   return EXIT_FAILURE;
  // }

  // v.seek( offset );

  // Process video
  toolbox::IOManager io_manager( "imgFile", "xFrameLists", "yFrameLists", flow_span, max_files_chunk, serialize );
  ProcessClip( inFramesList , xFrameLists , yFrameLists , io_manager, type, bound );

  std::cout << "\t .. Finished" << std::endl;
  return EXIT_SUCCESS;
}

void ProcessClip( std::string inFramesList , 	 std::string  xFrameLists ,  std::string yFrameLists   , toolbox::IOManager & io_manager, const int type, const int bound ){
  
    std::ifstream input_inframes( inFramesList.c_str() );
	std::string line_inframes;
	
	std::ifstream out_xframes( xFrameLists.c_str() );
	std::string line_xframes;
	
	std::ifstream out_yframes( yFrameLists.c_str() );
	std::string line_yframes;
	
	
	
  // At each processing step, we require `span+1` frames to compute flow
  auto max_span = std::max_element( flow_span.begin(), flow_span.end() );    

  // std::vector< std::pair<int64_t, cv::Mat> > clip;
  // v.read( clip, *max_span, true );
  cv::Mat frame , prev_image ;
  
  
  
  int64_t counter = 0;

  std::cout << "\t" << std::flush;
  
  getline( input_inframes, line_inframes );
  frame = cv::imread(line_inframes);
  if( resize_first )
      cv::resize( frame , frame, cv::Size( resize_first_width, resize_first_height ) );
      

  while( true ){
    // v.read( clip, 1, true );

    // if( clip.empty() )
    //   break;
    
    frame.copyTo(prev_image);
    
    if(!(getline( input_inframes, line_inframes )))
       		 break;
       		 
    frame = cv::imread(line_inframes);
    if( resize_first )
      cv::resize( frame , frame, cv::Size( resize_first_width, resize_first_height ) );

    if( ++counter % 50 == 0 )
      std::cout << " -- " << counter << std::flush;

    // io_manager.WriteImg( clip[0].second, counter );
     
    

      cv::Mat flow_x( cv::Size( DIM_X, DIM_Y ),   CV_8UC1 );
      cv::Mat flow_y( cv::Size( DIM_X, DIM_Y ),   CV_8UC1 );

      cv::Mat grey_first, grey_second;
      cv::cvtColor(prev_image,         grey_first, CV_BGR2GRAY );
      cv::cvtColor( frame,  grey_second, CV_BGR2GRAY );

      ComputeFlow( grey_first, grey_second, type, bound, flow_x, flow_y );
      
      getline(  out_xframes , line_xframes );
      getline( out_yframes , line_yframes );

      io_manager.WriteFlow( flow_x, flow_y, counter, 0 , line_xframes , line_yframes );
    

    // clip.erase( clip.begin(), clip.begin()+1 );
  }

  std::cout << " -- " << counter << "." << std::endl;

  io_manager.sync();
}

void ComputeFlow( const cv::Mat prev, const cv::Mat cur, const int type, const int bound, cv::Mat & flow_x, cv::Mat & flow_y ){

  // GPU optical flow
  cv::cuda::GpuMat frame_0( prev );
  cv::cuda::GpuMat frame_1( cur );

  cv::cuda::GpuMat d_flow( frame_0.size(), CV_32FC2 );

  switch(type){
  case 0:
    alg_farn->calc(frame_0, frame_1, d_flow );
    break;
  case 1:
    alg_tvl1->calc(frame_0, frame_1, d_flow );
    break;
  case 2:
    cv::cuda::GpuMat d_frame0f, d_frame1f;
    frame_0.convertTo(d_frame0f, CV_32F, 1.0 / 255.0);
    frame_1.convertTo(d_frame1f, CV_32F, 1.0 / 255.0);

    alg_brox->calc(d_frame0f, d_frame1f, d_flow );
    break;
  }

  cv::Mat flow( d_flow );
  cv::Mat tmp_flow[2];

  cv::split( flow, tmp_flow );

  cv::Mat imgX, imgY;
  cv::resize( tmp_flow[0], imgX, cv::Size( DIM_X, DIM_Y ) );
  cv::resize( tmp_flow[1], imgY, cv::Size( DIM_X, DIM_Y ) );

	#ifdef TEST_FINEGRAINED
	toolbox::convertFlowToImage( imgX, imgY, flow_x, flow_y, 0, 0, true );
	#else
	toolbox::convertFlowToImage( imgX, imgY, flow_x, flow_y, -bound, bound );
	#endif
}