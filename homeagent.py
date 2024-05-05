import json
from qwen_agent.llm import get_chat_model
import streamlit as st
from server import HomeRPC

def function_call(func, place = None, device_type = None, device_id = None, input_type = None):
    # Call the HomeRPC function
    if not (func and place and device_type and device_id):
        return json.dumps({
            "status": "error",
            "message": "Missing required parameters"
        })

    if input_type:
        ret = HomeRPC.place(place).device(device_type).id(device_id).call(func, input_type)
    else:
        ret = HomeRPC.place(place).device(device_type).id(device_id).call(func)
    
    if ret is None:
        return json.dumps({
            "status": "error",
            "message": "Function call failed"
        })
    
    return json.dumps({
        "status": "success",
        "return": ret
    })

if __name__ == "__main__":
    st.set_page_config(
        page_title="HomeAgent",
        page_icon=":robot:",
        layout="wide"
    )

    @st.cache_resource
    def get_model():
        llm = get_chat_model({
            'model': 'qwen-max',
            'model_server': 'dashscope',
            'api_key': 'xxxxx-xxxxx-xxxxx-xxxxx-xxxxx-xxxxx-xxxxx-xxxxx',
            'generate_cfg': {
                'top_p': 0.8,
                'temperature': 0.8
            }
        })

        HomeRPC.setup(ip = "192.168.43.9", log = True)

        return llm

    llm = get_model()

    if "history" not in st.session_state:
        st.session_state.history = []

    st.sidebar.header("HomeAgent", divider = False)

    st.sidebar.divider()

    funcName = st.sidebar.markdown("**函数调用：**")
    funcArgs = st.sidebar.markdown("**参数：**")
    callStatus = st.sidebar.markdown("**调用状态：**")

    st.sidebar.divider()

    buttonClean = st.sidebar.button("清理会话历史", key="clean")
    if buttonClean:
        st.session_state.history = []
        st.rerun()

    for i, message in enumerate(st.session_state.history):
        if message["role"] == "user":
            with st.chat_message(name="user", avatar="user"):
                st.markdown(message["content"])
        elif message["role"] == "assistant":
            if message["content"]:
                with st.chat_message(name="assistant", avatar="assistant"):
                    st.markdown(message["content"])

    with st.chat_message(name="user", avatar="user"):
        input_placeholder = st.empty()
    with st.chat_message(name="assistant", avatar="assistant"):
        message_placeholder = st.empty()

    prompt_text = st.chat_input("需要什么帮助吗？")

    if prompt_text:
        
        st.session_state.history.append({
            "role": "user",
            "content": prompt_text
        })

        input_placeholder.markdown(prompt_text)
        history = st.session_state.history
        responses = []
        
        for responses in llm.chat(messages=history,
                                    functions=HomeRPC.funcs(),
                                    stream=True):
            if responses[0]["content"]:
                message_placeholder.markdown(responses[0]["content"])
        
        history.extend(responses)

        last_response = history[-1]

        if last_response.get('function_call', None):

            try:
                function_name = last_response['function_call']['name']
                funcName.markdown(f"**函数调用：**```{function_name}```")
                function_args = json.loads(last_response['function_call']['arguments'])
                funcArgs.markdown(f"**参数：**```{function_args}```")
        
                function_response = function_call(
                    function_name,
                    **function_args
                )

                if json.loads(function_response).get('status') == 'success':
                    callStatus.markdown(f"**调用状态：**<span style='color:green'>```{function_response}```</span>", unsafe_allow_html=True)
                else:
                    callStatus.markdown(f"**调用状态：**<span style='color:red'>```{function_response}```</span>", unsafe_allow_html=True)

                history.append({
                    'role': 'function',
                    'name': function_name,
                    'content': function_response
                })

                for responses in llm.chat(
                            messages=history,
                            functions=HomeRPC.funcs(),
                            stream=True,
                    ):
                        message_placeholder.markdown(responses[0]["content"])
                    
                history.extend(responses)

            except Exception as e:

                callStatus.markdown(f'**调用状态：**<span style="color:red">```{e}```</span>', unsafe_allow_html=True)